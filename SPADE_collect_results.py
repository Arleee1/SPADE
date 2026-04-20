#!/usr/bin/env python3
"""Collect SPADE copy_grid_halo runtimes and emit LaTeX heatmap tables.

For every (config, width, height), this script runs stencil-grid twice:
  1) -a STENCIL_9_POINT
  2) -a LEAST_USED_CORES

It extracts Runtime(ms) from the command-stats row:
  copy_grid_halo.fp32.h

If that row is missing, runtime is recorded as -1.

Speedup in each heatmap cell is defined as:
  speedup = runtime(LEAST_USED_CORES) / runtime(STENCIL_9_POINT)

So speedup > 1 means STENCIL_9_POINT is faster.

Radius and iterations are intentionally fixed to launch.json-style values:
  -r 1
  -n 5
"""

from __future__ import annotations

import argparse
import csv
import os
import re
import shlex
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Sequence, Tuple


# ---------------------------
# Easy-to-edit defaults
# ---------------------------
DEFAULT_CONFIG_NAMES = [
	"PIMeval_Bank_Rank16.cfg",
	"PIMeval_Bank_Rank8.cfg",
	"PIMeval_Fulcrum_Rank16.cfg",
  "PIMeval_Fulcrum_Rank8.cfg",
]

DEFAULT_WIDTHS = [10, 100, 1_000, 10_000]
DEFAULT_HEIGHTS = [10, 100, 1_000, 10_000]
ANALYSIS_EXTRA_SIZES = [100_000, 1_000_000, 10_000_000]

STRATEGY_STENCIL = "STENCIL_9_POINT"
STRATEGY_LEAST_USED = "LEAST_USED_CORES"
FIXED_RADIUS = 1
FIXED_ITERATIONS = 5


COPY_GRID_HALO_RUNTIME_PATTERN = re.compile(
	r"^\s*copy_grid_halo\.fp32\.h\s*:\s+\d+\s+"
	r"([0-9]+(?:\.[0-9]+)?(?:[eE][+-]?\d+)?)\b",
	re.MULTILINE,
)


@dataclass
class StrategyRun:
	command_index: int
	total_commands: int
	config_name: str
	strategy: str
	width: int
	height: int
	command: str
	return_code: int
	runtime_ms: float
	status: str
	log_file: str


@dataclass
class CommandResult:
	return_code: int
	output: str
	timed_out: bool


@dataclass
class HeatmapCell:
	config_name: str
	width: int
	height: int
	runtime_stencil_ms: float
	runtime_least_used_ms: float
	speedup_stencil_vs_least_used: float
	status: str
	stencil_log_file: str
	least_used_log_file: str


def parse_copy_grid_halo_runtime(output: str) -> float:
	"""Return copy_grid_halo.fp32.h Runtime(ms), or -1 if not found."""
	match = COPY_GRID_HALO_RUNTIME_PATTERN.search(output)
	if not match:
		return -1.0
	return float(match.group(1))


def run_command(
	command: Sequence[str],
	cwd: Path,
	env: Dict[str, str],
	timeout_seconds: int,
) -> CommandResult:
	"""Run a command and capture combined stdout/stderr.

	If timeout_seconds > 0 and the command exceeds it, mark the result timed out.
	"""
	try:
		result = subprocess.run(
			list(command),
			cwd=str(cwd),
			env=env,
			stdout=subprocess.PIPE,
			stderr=subprocess.STDOUT,
			text=True,
			check=False,
			timeout=(timeout_seconds if timeout_seconds > 0 else None),
		)
		return CommandResult(return_code=result.returncode, output=result.stdout, timed_out=False)
	except subprocess.TimeoutExpired as exc:
		timeout_output = (exc.stdout or "")
		if isinstance(timeout_output, bytes):
			timeout_output = timeout_output.decode("utf-8", errors="replace")
		return CommandResult(return_code=124, output=timeout_output, timed_out=True)


def ensure_parent(path: Path) -> None:
	path.parent.mkdir(parents=True, exist_ok=True)


def per_config_output_path(base_path: Path, config_name: str) -> Path:
	"""Derive a per-config output log path from a base path."""
	suffix = base_path.suffix if base_path.suffix else ".txt"
	return base_path.with_name(f"{base_path.stem}_{Path(config_name).stem}{suffix}")


def latex_escape(text: str) -> str:
	"""Escape a string for LaTeX text context."""
	replacements = {
		"\\": r"\textbackslash{}",
		"_": r"\_",
		"&": r"\&",
		"%": r"\%",
		"$": r"\$",
		"#": r"\#",
		"{": r"\{",
		"}": r"\}",
		"~": r"\textasciitilde{}",
		"^": r"\textasciicircum{}",
	}
	out = text
	for old, new in replacements.items():
		out = out.replace(old, new)
	return out


def format_speedup_cell(speedup: float) -> str:
	if speedup < 0:
		return r"\textbf{ERR}"
	return f"{speedup:.2f}"


def discover_taco_configs(config_dir: Path) -> List[str]:
	return sorted(path.name for path in config_dir.glob("*.cfg"))


def resolve_config_paths(config_dir: Path, config_entries: Sequence[str]) -> List[Path]:
	paths: List[Path] = []
	for entry in config_entries:
		candidate = Path(entry)
		if candidate.is_absolute():
			path = candidate
		else:
			path = config_dir / candidate
		if not path.exists():
			raise FileNotFoundError(f"Config not found: {path}")
		paths.append(path.resolve())
	return paths


def build_latex_heatmaps(
	cells: Sequence[HeatmapCell],
	config_names: Sequence[str],
	widths: Sequence[int],
	heights: Sequence[int],
) -> str:
	cell_map: Dict[Tuple[str, int, int], HeatmapCell] = {
		(cell.config_name, cell.width, cell.height): cell for cell in cells
	}

	valid_speedups = [
		cell.speedup_stencil_vs_least_used
		for cell in cells
		if cell.speedup_stencil_vs_least_used > 0
	]
	max_speedup = max(valid_speedups) if valid_speedups else 1.0

	lines: List[str] = []
	lines.append("% Auto-generated by SPADE_collect_results.py")
	lines.append("% Requires: \\usepackage{booktabs}")
	lines.append("% Optional heatmap plot package:")
	lines.append("%   \\usepackage{pgfplots}")
	lines.append("%   \\pgfplotsset{compat=1.18}")
	lines.append(
		"% Speedup definition: runtime(LEAST_USED_CORES) / runtime(STENCIL_9_POINT)"
	)
	lines.append("")

	for config_name in config_names:
		# Numeric table for direct paper inclusion.
		lines.append(r"\begin{table}[htbp]")
		lines.append(r"\centering")
		lines.append(
			r"\caption{Speedup heatmap values for "
			+ latex_escape(config_name)
			+ r" (STENCIL\_9\_POINT vs LEAST\_USED\_CORES)}"
		)
		lines.append(r"\begin{tabular}{r|" + "c" * len(widths) + r"}")
		lines.append(r"\toprule")
		lines.append(
			r"\multicolumn{1}{r|}{Height} & \multicolumn{"
			+ str(len(widths))
			+ r"}{c}{Width} \\"
		)
		lines.append(" & ".join([""] + [str(w) for w in widths]) + r" \\")
		lines.append(r"\midrule")

		for h in heights:
			row_cells = [str(h)]
			for w in widths:
				cell = cell_map.get((config_name, w, h))
				if cell is None:
					row_cells.append(r"\textbf{ERR}")
				else:
					row_cells.append(format_speedup_cell(cell.speedup_stencil_vs_least_used))
			lines.append(" & ".join(row_cells) + r" \\")

		lines.append(r"\bottomrule")
		lines.append(r"\end{tabular}")
		lines.append(r"\end{table}")
		lines.append("")

		# Optional PGFPlots heatmap snippet.
		lines.append(r"\begin{figure}[htbp]")
		lines.append(r"\centering")
		lines.append(r"\begin{tikzpicture}")
		lines.append(
			r"\begin{axis}["
			r"title={"
			+ latex_escape(config_name)
			+ r"},"
			r"xlabel={Width},"
			r"ylabel={Height},"
			r"xtick={"
			+ ",".join(str(w) for w in widths)
			+ r"},"
			r"ytick={"
			+ ",".join(str(h) for h in heights)
			+ r"},"
			r"colorbar,"
			r"colormap/viridis,"
			r"point meta min=0,"
			rf"point meta max={max_speedup:.6g}"
			r"]"
		)
		lines.append(r"\addplot[matrix plot*, mesh/cols=" + str(len(widths)) + r"] table[meta=speedup] {")
		lines.append("x y speedup")
		for h in heights:
			for w in widths:
				cell = cell_map.get((config_name, w, h))
				if cell is None or cell.speedup_stencil_vs_least_used < 0:
					speedup_text = "nan"
				else:
					speedup_text = f"{cell.speedup_stencil_vs_least_used:.6g}"
				lines.append(f"{w} {h} {speedup_text}")
		lines.append(r"};")
		lines.append(r"\end{axis}")
		lines.append(r"\end{tikzpicture}")
		lines.append(
			r"\caption{Heatmap of speedup (runtime(LEAST\_USED\_CORES) / runtime(STENCIL\_9\_POINT)) for "
			+ latex_escape(config_name)
			+ r". Cells with missing copy\_grid\_halo are NaN/ERR.}"
		)
		lines.append(r"\end{figure}")
		lines.append("")

	return "\n".join(lines)


def main() -> int:
	repo_root = Path(__file__).resolve().parent
	config_dir = repo_root / "configs" / "taco"
	default_program = repo_root / "misc-bench/stencil/PIM/stencil-grid.out"
	default_cwd = default_program.parent

	parser = argparse.ArgumentParser(
		description=(
			"Run SPADE stencil-grid sweeps and collect copy_grid_halo.fp32.h runtime "
			"for STENCIL_9_POINT vs LEAST_USED_CORES."
		)
	)
	parser.add_argument(
		"--program",
		default=str(default_program),
		help="Path to stencil-grid executable.",
	)
	parser.add_argument(
		"--cwd",
		default=str(default_cwd),
		help="Working directory for commands.",
	)
	parser.add_argument(
		"--config-dir",
		default=str(config_dir),
		help="Directory that contains taco config files (.cfg).",
	)
	parser.add_argument(
		"--list-configs",
		action="store_true",
		help="List available configs from --config-dir and exit.",
	)
	parser.add_argument(
		"--configs",
		nargs="+",
		default=DEFAULT_CONFIG_NAMES,
		help=(
			"Config filenames (or absolute paths) to sweep. "
			"By default uses an editable list near the top of this script."
		),
	)
	parser.add_argument(
		"--widths",
		nargs="+",
		type=int,
		default=DEFAULT_WIDTHS,
		help="Widths to sweep (x values).",
	)
	parser.add_argument(
		"--heights",
		nargs="+",
		type=int,
		default=DEFAULT_HEIGHTS,
		help="Heights to sweep (y values).",
	)
	parser.add_argument(
		"--output-csv",
		default=str(repo_root / "spade_strategy_speedup_results.csv"),
		help="Output CSV filename/path (saved under a mode-specific results folder).",
	)
	parser.add_argument(
		"--output-tex",
		default=str(repo_root / "spade_strategy_speedup_heatmaps.tex"),
		help="Output LaTeX filename/path (saved under a mode-specific results folder).",
	)
	parser.add_argument(
		"--output-text",
		default=str(repo_root / "spade_all_runs_output.txt"),
		help=(
			"Base output text filename/path. One text file is written per config as "
			"<stem>_<config><suffix>."
		),
	)
	parser.add_argument(
		"--log-dir",
		default=str(repo_root / "spade_collect_logs"),
		help="Log directory name/path (created under a mode-specific results folder).",
	)
	parser.add_argument(
		"--print-output",
		action="store_true",
		help="Print each command output to stdout.",
	)
	parser.add_argument(
		"--timeout-seconds",
		type=int,
		default=0,
		help="Optional timeout per command in seconds; 0 means no timeout.",
	)
	parser.add_argument(
		"--analysis-mode",
		action="store_true",
		help=(
			"Enable analysis mode: include 100k/1M sizes and set "
			"PIMEVAL_ANALYSIS_MODE=1."
		),
	)

	args = parser.parse_args()
	mode_name = "analysis" if args.analysis_mode else "regular"
	mode_results_root = (repo_root / "spade_results" / mode_name).resolve()

	program = Path(args.program).resolve()
	cwd = Path(args.cwd).resolve()
	output_csv = (mode_results_root / Path(args.output_csv).name).resolve()
	output_tex = (mode_results_root / Path(args.output_tex).name).resolve()
	output_text = (mode_results_root / Path(args.output_text).name).resolve()
	log_dir = (mode_results_root / Path(args.log_dir).name).resolve()
	config_dir = Path(args.config_dir).resolve()

	available_configs = discover_taco_configs(config_dir)
	if args.list_configs:
		print(f"Configs in {config_dir}:")
		for name in available_configs:
			print(f"  {name}")
		return 0

	if not program.exists():
		raise FileNotFoundError(f"Program not found: {program}")
	if not cwd.exists():
		raise FileNotFoundError(f"Working directory not found: {cwd}")
	if not config_dir.exists():
		raise FileNotFoundError(f"Config directory not found: {config_dir}")

	config_paths = resolve_config_paths(config_dir, args.configs)
	config_names = [path.name for path in config_paths]

	widths = sorted(set(args.widths))
	heights = sorted(set(args.heights))
	extra_sizes = set(ANALYSIS_EXTRA_SIZES)
	if args.analysis_mode:
		widths = sorted(set(widths) | extra_sizes)
		heights = sorted(set(heights) | extra_sizes)
	else:
		widths = sorted(w for w in widths if w not in extra_sizes)
		heights = sorted(h for h in heights if h not in extra_sizes)
	if not widths or not heights:
		raise ValueError("--widths and --heights must be non-empty")

	env = os.environ.copy()
	# Load balancing is incompatible with this allocation flow.
	env["PIMEVAL_LOAD_BALANCE"] = "0"
	env["PIMEVAL_ANALYSIS_MODE"] = "1" if args.analysis_mode else "0"

	ensure_parent(output_csv)
	ensure_parent(output_tex)
	log_dir.mkdir(parents=True, exist_ok=True)

	per_config_output_files: Dict[str, Path] = {
		config_path.name: per_config_output_path(output_text, config_path.name)
		for config_path in config_paths
	}
	for per_config_output in per_config_output_files.values():
		ensure_parent(per_config_output)

	runs: List[StrategyRun] = []
	command_index = 0
	strategies = [STRATEGY_STENCIL, STRATEGY_LEAST_USED]
	total_commands = len(config_paths) * len(heights) * len(widths) * len(strategies)

	for config_path in config_paths:
		config_output_text = per_config_output_files[config_path.name]
		with config_output_text.open("w", encoding="utf-8") as config_output_file:
			for height in heights:
				for width in widths:
					for strategy in strategies:
						command_index += 1
						command = [
							str(program),
							"-a",
							strategy,
							"-x",
							str(width),
							"-y",
							str(height),
							"-r",
							str(FIXED_RADIUS),
							"-n",
							str(FIXED_ITERATIONS),
							"-c",
							str(config_path),
						]
						if not args.analysis_mode:
							command.extend(["-v", "t"])

						command_text = " ".join(shlex.quote(part) for part in command)
						print(
							f"[{command_index}/{total_commands}] Running {strategy} "
							f"config={config_path.name} x={width} y={height}",
							flush=True,
						)
						config_output_file.write(
							f"RUN {command_index}/{total_commands}\n"
							f"config={config_path.name}\n"
							f"strategy={strategy}\n"
							f"width={width}\n"
							f"height={height}\n"
							"command=\n"
							f"{command_text}\n"
						)
						config_output_file.flush()

						start_time = time.monotonic()
						result = run_command(
							command=command,
							cwd=cwd,
							env=env,
							timeout_seconds=args.timeout_seconds,
						)
						elapsed_s = time.monotonic() - start_time

						output = result.output
						runtime_ms = parse_copy_grid_halo_runtime(output)
						if result.timed_out:
							status = "timed_out"
						elif runtime_ms >= 0:
							status = "ok"
						else:
							status = "missing_copy_grid_halo"

						log_file = log_dir / (
							f"command_{command_index:04d}_{config_path.stem}_{strategy}_x{width}_y{height}.log"
						)
						log_file.write_text(output, encoding="utf-8")

						if args.print_output:
							print(f"===== Command {command_index} Output =====", flush=True)
							print(output, flush=True)

						config_output_file.write(
							f"return_code={result.return_code}\n"
							f"timed_out={result.timed_out}\n"
							f"elapsed_seconds={elapsed_s:.2f}\n"
							"output=\n"
							f"{output.rstrip()}\n\n"
						)
						config_output_file.flush()

						print(
							f"[{command_index}/{total_commands}] Done status={status} "
							f"copy_grid_halo_runtime_ms={runtime_ms} elapsed={elapsed_s:.2f}s",
							flush=True,
						)

						runs.append(
							StrategyRun(
								command_index=command_index,
								total_commands=total_commands,
								config_name=config_path.name,
								strategy=strategy,
								width=width,
								height=height,
								command=command_text,
								return_code=result.return_code,
								runtime_ms=runtime_ms,
								status=status,
								log_file=str(log_file),
							)
						)

	run_map: Dict[Tuple[str, int, int, str], StrategyRun] = {
		(run.config_name, run.width, run.height, run.strategy): run for run in runs
	}

	cells: List[HeatmapCell] = []
	for config_name in config_names:
		for height in heights:
			for width in widths:
				stencil_run = run_map.get((config_name, width, height, STRATEGY_STENCIL))
				least_used_run = run_map.get((config_name, width, height, STRATEGY_LEAST_USED))

				runtime_stencil = stencil_run.runtime_ms if stencil_run is not None else -1.0
				runtime_least_used = least_used_run.runtime_ms if least_used_run is not None else -1.0

				if runtime_stencil > 0 and runtime_least_used > 0:
					speedup = runtime_least_used / runtime_stencil
					status = "ok"
				elif runtime_stencil <= 0 and runtime_least_used <= 0:
					speedup = -1.0
					status = "missing_both"
				elif runtime_stencil <= 0:
					speedup = -1.0
					status = "missing_stencil"
				else:
					speedup = -1.0
					status = "missing_least_used"

				cells.append(
					HeatmapCell(
						config_name=config_name,
						width=width,
						height=height,
						runtime_stencil_ms=runtime_stencil,
						runtime_least_used_ms=runtime_least_used,
						speedup_stencil_vs_least_used=speedup,
						status=status,
						stencil_log_file=(stencil_run.log_file if stencil_run is not None else ""),
						least_used_log_file=(least_used_run.log_file if least_used_run is not None else ""),
					)
				)

	with output_csv.open("w", newline="", encoding="utf-8") as csv_file:
		writer = csv.DictWriter(
			csv_file,
			fieldnames=[
				"config_name",
				"width",
				"height",
				"runtime_stencil_9_point_ms",
				"runtime_least_used_cores_ms",
				"speedup_stencil_9_point_vs_least_used_cores",
				"status",
				"stencil_log_file",
				"least_used_log_file",
			],
		)
		writer.writeheader()
		for cell in cells:
			writer.writerow(
				{
					"config_name": cell.config_name,
					"width": cell.width,
					"height": cell.height,
					"runtime_stencil_9_point_ms": cell.runtime_stencil_ms,
					"runtime_least_used_cores_ms": cell.runtime_least_used_ms,
					"speedup_stencil_9_point_vs_least_used_cores": cell.speedup_stencil_vs_least_used,
					"status": cell.status,
					"stencil_log_file": cell.stencil_log_file,
					"least_used_log_file": cell.least_used_log_file,
				}
			)

	latex = build_latex_heatmaps(
		cells=cells,
		config_names=config_names,
		widths=widths,
		heights=heights,
	)
	output_tex.write_text(latex, encoding="utf-8")

	print(f"Wrote {len(cells)} heatmap cells to: {output_csv}")
	print(f"Wrote LaTeX heatmap tables/snippets to: {output_tex}")
	print(f"Results folder: {mode_results_root}")
	print("Wrote per-config run output text files:")
	for config_name in config_names:
		print(f"  {config_name}: {per_config_output_files[config_name]}")
	print(
		"Fixed run parameters: "
		f"-a [{STRATEGY_STENCIL}, {STRATEGY_LEAST_USED}], "
		f"-r {FIXED_RADIUS}, -n {FIXED_ITERATIONS}, "
		f"analysis_mode={args.analysis_mode}, "
		f"PIMEVAL_ANALYSIS_MODE={env['PIMEVAL_ANALYSIS_MODE']}, "
		f"-v t {'disabled' if args.analysis_mode else 'enabled'}"
	)
	print("Summary:")
	for config_name in config_names:
		ok_cells = sum(
			1
			for cell in cells
			if cell.config_name == config_name and cell.speedup_stencil_vs_least_used > 0
		)
		total_cells = sum(1 for cell in cells if cell.config_name == config_name)
		print(f"  {config_name}: {ok_cells}/{total_cells} cells with valid speedup")

	return 0


if __name__ == "__main__":
	sys.exit(main())
