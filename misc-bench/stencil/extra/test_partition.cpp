#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "../../../util/utilGrid.h"

struct PartitionCase {
	std::string name;
	uint64_t gridWidth;
	uint64_t gridHeight;
	uint64_t maxAvailableCores;
	uint64_t maxTileWidth;
	uint64_t maxTileHeight;
};

bool runPartitionCase(const PartitionCase& partitionCase) {
	const GridPartitioning partitioning = calculateGridPartitioning(
			partitionCase.gridWidth,
			partitionCase.gridHeight,
			partitionCase.maxAvailableCores,
			partitionCase.maxTileWidth,
			partitionCase.maxTileHeight);

	const uint64_t coveredWidth =
			(partitioning.numCoresHorizontal - 1) * partitioning.tileWidth +
			partitioning.tileWidthLast;
	const uint64_t coveredHeight =
			(partitioning.numCoresVertical - 1) * partitioning.tileHeight +
			partitioning.tileHeightLast;
	const uint64_t expectedWidth = partitionCase.gridWidth;
	const uint64_t expectedHeight = partitionCase.gridHeight;
	const bool widthMatches = coveredWidth == expectedWidth;
	const bool heightMatches = coveredHeight == expectedHeight;
	const bool coresWithinLimit = partitioning.totalCores <= partitionCase.maxAvailableCores;
	const bool tileWidthWithinLimit = partitioning.tileWidth <= partitionCase.maxTileWidth;
	const bool tileHeightWithinLimit = partitioning.tileHeight <= partitionCase.maxTileHeight;
	const bool tileWidthLastWithinLimit = partitioning.tileWidthLast <= partitionCase.maxTileWidth;
	const bool tileHeightLastWithinLimit = partitioning.tileHeightLast <= partitionCase.maxTileHeight;

	std::string failureReason;
	auto appendFailureReason = [&failureReason](const std::string& reason) {
		if (!failureReason.empty()) {
			failureReason += "; ";
		}
		failureReason += reason;
	};

	if (!widthMatches) {
		appendFailureReason("covered width != expected width");
	}
	if (!heightMatches) {
		appendFailureReason("covered height != expected height");
	}
	if (!coresWithinLimit) {
		appendFailureReason("used cores > max cores");
	}
	if (!tileWidthWithinLimit || !tileWidthLastWithinLimit) {
		appendFailureReason("tile width exceeds max tile width");
	}
	if (!tileHeightWithinLimit || !tileHeightLastWithinLimit) {
		appendFailureReason("tile height exceeds max tile height");
	}

	const bool testSucceeded = failureReason.empty();

	std::cout << "Case: " << partitionCase.name << '\n';
	std::cout << "  Input: grid=" << partitionCase.gridWidth << "x" << partitionCase.gridHeight
						<< ", maxCores=" << partitionCase.maxAvailableCores
						<< ", maxTile=" << partitionCase.maxTileWidth << "x" << partitionCase.maxTileHeight << '\n';
	std::cout << "  Output: cores=" << partitioning.totalCores
						<< " (" << partitioning.numCoresHorizontal << "x" << partitioning.numCoresVertical << ")"
						<< ", tile=" << partitioning.tileWidth << "x" << partitioning.tileHeight
						<< ", tileLast=" << partitioning.tileWidthLast << "x" << partitioning.tileHeightLast << '\n';
	std::cout << "  Coverage: " << coveredWidth << "x" << coveredHeight
						<< " (expected=" << expectedWidth << "x" << expectedHeight << ")" << '\n';
	std::cout << "  Result: " << (testSucceeded ? "PASS" : ("FAIL: " + failureReason)) << "\n\n";

	return testSucceeded;
}

int main() {
	const std::vector<PartitionCase> partitionCases = {
			{"Square16", 4096, 4096, 16, 1200, 1200},
			{"Wide20", 5000, 3000, 20, 1300, 1200},
			{"Video24", 7680, 4320, 24, 1600, 1200},
			{"Uneven12", 4097, 2049, 12, 1200, 1200},
			{"Tall18", 3000, 7000, 18, 1200, 1400},
      {"1000", 1000, 1000, 512, 1000, 8000},
	};

	uint64_t numPassed = 0;
	for (const PartitionCase& partitionCase : partitionCases) {
		if (runPartitionCase(partitionCase)) {
			++numPassed;
		}
	}

	const uint64_t numFailed = partitionCases.size() - numPassed;
	if (numFailed == 0) {
		std::cout << "Summary: " << numPassed << "/" << partitionCases.size() << " tests passed" << '\n';
	} else {
		std::cout << "Summary: " << numPassed << "/" << partitionCases.size()
						<< " tests passed, " << numFailed << " failed (see FAIL reasons above)" << '\n';
	}

	return (numPassed == partitionCases.size()) ? 0 : 1;
}
