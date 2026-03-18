#include <iostream>
#include <filesystem>
#include <string>
#include <fstream>
#include <sstream>
#include <cmath>

using namespace std;

const int START_OFFSET = 14; // actual numerical data start on line 14
const int LAST_LINE = 1037; // 1037 line or 1.0 is always final
constexpr double STEP = 1.0 / 1023.0;
const double DELTA = 0.0001;
const double LAST_VALUE = 1.0;
const int DEFAULT_LUT_SIZE = 1024;

int main(int argc, char* argv[]) {
	if ((argc < 3) || (argc > 9)) {
		cout << "Wrong format\n\n";
		//if (argc == 2 && (argv[1] == "help" || argv[1] == "-help")) {
			cout << "Use \"./cci.exe \"path to file or filename if file is in current folder\"\n\n"
				<< "\"start position of interpolation (line number or float, or curve pos from DisplayCal's curve viewer)\"\n\n"
				<< "(optional (if nothing, assumes \"1.0\" or \"1037\")) after end position of interpolation (line number or float)\n\n"
				<< "(optional (if nothing, assumes default)) change operation mode:\n		0 - default, interpolate between start and 1.0;\n		1 - continue current trajectory and curve to 1.0 near end\n\n"
				<< "(optional (if nothing, assumes 100, 100, 100)) per channel rgb value modification:\n		r g b (example: 50 50 50 or 85 88 92)\n" << endl;
		//}
		return 0;
	}

	auto doubleAreEqual = [](double first, double second) { return abs(first - second) < DELTA; };

	filesystem::path filePath = argv[1];
	if (!filesystem::exists(filePath)) { cout << "File path or filename does NOT exist" << endl; return 0; }
	if (filePath.extension() != ".cal") { cout << "Only \".cal\" files are supported" << endl; return 0; }

	double startPos = stod(argv[2]);
	if (startPos < 0) { cout << "Start position cannot be a negative number" << endl; return 0; }
	if (string(argv[2]).find('.') != string::npos && (startPos > 1.0)) {
		startPos /= 255;
		//cout << startPos << endl;
	}
	else if (startPos >= 1.0) { cout << "Start position cannot be \"1\" or \"1.0\" or more" << endl; return 0; }

	double afterEndPos = 1.0;
	if (argc >= 4) {
		double temp = stod(argv[3]);
		if (temp < 0 || doubleAreEqual(temp, 0.0)) { cout << "After end position cannot be negative or a zero" << endl; return 0; }
		if (string(argv[3]).find('.') != string::npos && (temp > 1.0)) {
			temp /= 255;
		}
		if (temp < startPos) { cout << "End pos cannot be smaller that start pos" << endl; return 0; }
		afterEndPos = temp;
	}
	
	enum OpMode {
		NORMAL,
		PARALLEL_TRAJECTORY
	};
	OpMode opMode = NORMAL;
	if ((argc >= 5) && stoi(argv[4]) == 1) {
		opMode = PARALLEL_TRAJECTORY;
	}

	bool isClamping = false;
	if ((argc >= 6) && stoi(argv[5]) == 1) {
		isClamping = true;
	}

	int userR = 100, userG = 100, userB = 100;
	if (argc >= 9) {
		userR = stoi(argv[6]);
		userG = stoi(argv[7]);
		userB = stoi(argv[8]);
		if (userR < 0 || userG < 0 || userB < 0) { cout << "RGB value modifications cannot be less that 0" << endl; return 0; }
	}

	filesystem::path origFilePath = filePath.generic_string() + ".orig";
	error_code copy_error;
	if (!filesystem::exists(origFilePath)) {
		filesystem::copy(filePath, origFilePath, copy_error);
		if (copy_error) { cout << "Backup creation failed. Error message: " << copy_error.message() << endl; return 0; }
	}

	double interpStart = (startPos < LAST_VALUE ? startPos : ((startPos - START_OFFSET) * STEP));
	double interpEnd = (afterEndPos <= LAST_VALUE ? afterEndPos : ((afterEndPos - START_OFFSET) * STEP));

	ifstream inOpenFile(filePath);
	if (!inOpenFile.good()) {
		throw runtime_error("input .cal file cannot be opened");
	}

	vector<tuple<double, double, double, double>> outputValues;
	outputValues.reserve(DEFAULT_LUT_SIZE);

	tuple<double, double, double> lastOrigRGB;

	stringstream outputSS;
	string input_tmp;

	while (getline(inOpenFile, input_tmp)) {
		if (input_tmp.empty() || *input_tmp.begin() != '0') {
			outputSS << input_tmp << endl;
			continue;
		}
		size_t endCursorPos = input_tmp.find_first_of(' ');
		double origPos = stod(input_tmp.substr(0, endCursorPos));

		size_t startCursorPos = input_tmp.find_first_not_of(' ', endCursorPos);
		endCursorPos = input_tmp.find_first_of(' ', startCursorPos);
		double origR = stod(input_tmp.substr(startCursorPos, (endCursorPos - startCursorPos)));

		startCursorPos = input_tmp.find_first_not_of(' ', endCursorPos);
		endCursorPos = input_tmp.find_first_of(' ', startCursorPos);
		double origG = stod(input_tmp.substr(startCursorPos, (endCursorPos - startCursorPos)));

		startCursorPos = input_tmp.find_first_not_of(' ', endCursorPos);
		endCursorPos = input_tmp.find_first_of(' ', startCursorPos);
		double origB = stod(input_tmp.substr(startCursorPos, (endCursorPos - startCursorPos)));

		if (doubleAreEqual(interpStart, origPos)) {
			interpStart = origPos;
			lastOrigRGB = make_tuple(origR, origG, origB);
			break;
		}
		outputValues.emplace_back(origPos, origR, origG, origB);
	}
	inOpenFile.close();

	double diff = abs(interpStart - get<0>(lastOrigRGB));
	double firstLerpEnd = (doubleAreEqual(interpEnd, LAST_VALUE) ? interpEnd - diff * 2 : interpEnd);

	if (opMode == PARALLEL_TRAJECTORY) {
		for (double i = interpStart; i < firstLerpEnd; i += STEP) {
			double parallelValue = i + diff;
			outputValues.emplace_back(i, parallelValue, parallelValue, parallelValue);
		}
	}


	const double newInterpStart = (opMode == PARALLEL_TRAJECTORY ? firstLerpEnd : interpStart);
	const double newFirst = (opMode == PARALLEL_TRAJECTORY ? newInterpStart + diff : get<0>(lastOrigRGB));
	const double newInterpEnd = min(1.0 , (interpEnd + (opMode == PARALLEL_TRAJECTORY ? diff : 0)));
	const double divider = newInterpEnd - newInterpStart;
	double prevLerpResult;
	for (double i = newInterpStart; (i < LAST_VALUE || doubleAreEqual(i, LAST_VALUE)); i += STEP) {
		double lerpResult = lerp(newFirst, newInterpEnd, (i - newInterpStart) / divider);
		prevLerpResult = ((isClamping && (i > newInterpEnd)) ? prevLerpResult : lerpResult);
		double input = ((isClamping && (i > newInterpEnd)) ? prevLerpResult : lerpResult);
		outputValues.emplace_back(i, input, input, input);
	}

	// use chosen lerp start point to calculate color offsets
	// calculate max channel
	double tpp = max(max(userR, userG), userB);
	// normalize channel offset values and calculate new values
	double tmpR = get<0>(lastOrigRGB) * ((double)userR / tpp);
	double tmpG = get<1>(lastOrigRGB) * ((double)userG / tpp);
	double tmpB = get<2>(lastOrigRGB) * ((double)userB / tpp);
	// calculate final "static" offsets
	double modifiedMaxChannel = max(max(tmpR, tmpG), tmpB);
	double rOffset = modifiedMaxChannel - tmpR;
	double gOffset = modifiedMaxChannel - tmpG;
	double bOffset = modifiedMaxChannel - tmpB;

	// tries to fix lost brightness due to changing rgb values, thus inherently losing some brightness
	// mb a bit crude, but better than nothing
	// ideally it should be based on our perception of light, and should be further researched
	const double brightnessOffset = (rOffset + gOffset + bOffset) / 3;

	filesystem::path outFileName = filePath.replace_filename(filePath.stem().generic_string() + "_interpolated" + filePath.extension().generic_string());
	ofstream outOpenFile(outFileName);
	if (!outOpenFile.good()) {
		throw runtime_error("output .cal file cannot be created");
	}

	filesystem::path lutFileName = filePath.replace_filename(filePath.stem().generic_string() + "_interpolated_1dlut.csv");
	ofstream lut1dOutFile(lutFileName);
	if (!lut1dOutFile.good()) {
		throw runtime_error("1dlut .csv file cannot be created");
	}

	outOpenFile << fixed;
	lut1dOutFile << fixed;

	string out_tmp;
	while (getline(outputSS, out_tmp)) {
			outOpenFile << out_tmp << endl;
	}

	int modStart = (int) ceil((max(rOffset, max(gOffset, bOffset)) - brightnessOffset) / get<1>(*(outputValues.begin() + 1)));
	int counter = 0;

	for (const auto& values : outputValues) {
		double combModifier = (double) counter / modStart;
		counter += (counter < modStart ? 1 : 0);

		double r = min(1.0, max(0.0, get<1>(values) - (rOffset - brightnessOffset) * combModifier));
		double g = min(1.0, max(0.0, get<2>(values) - (gOffset - brightnessOffset) * combModifier));
		double b = min(1.0, max(0.0, get<3>(values) - (bOffset - brightnessOffset) * combModifier));

		outOpenFile << setprecision(14) << std::left << setfill('0') << setw(16) << get<0>(values) << ' ';
		outOpenFile << setprecision(15);
		outOpenFile << std::left << setfill('0') << setw(17) << r << ' ';
		outOpenFile << std::left << setfill('0') << setw(17) << g << ' ';
		outOpenFile << std::left << setfill('0') << setw(17) << b;
		outOpenFile << endl;

		lut1dOutFile << setprecision(15);
		lut1dOutFile << std::left << setfill('0') << setw(17) << r << ',';
		lut1dOutFile << std::left << setfill('0') << setw(17) << g << ',';
		lut1dOutFile << std::left << setfill('0') << setw(17) << b;
		lut1dOutFile << endl;
	}
	outOpenFile << "END_DATA";
}