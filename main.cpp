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
const int END_LERP_START = 50;
constexpr double LAST_STEP = 1.0;

int main(int argc, char* argv[]) {
	if ((argc < 3) || (argc > 8)) {
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

	filesystem::path filePath = argv[1];
	if (!filesystem::exists(filePath)) { cout << "File path or filename does NOT exist" << endl; return 0; }
	if (filePath.extension() != ".cal") { cout << "Only \".cal\" files are supported" << endl; return 0; }

	double startPos = stod(argv[2]);
	if (string(argv[2]).find('.') != string::npos && (startPos > 1.0)) {
		startPos = startPos / 256.0;
		cout << startPos << endl;
	}
	if (startPos < 0) { cout << "Start position cannot be a negative number" << endl; return 0; }
	else if (startPos == 1) { cout << "Start position cannot be \"1\" or \"1.0\"" << endl; return 0; }

	double afterEndPos = 1.0;
	if (argc >= 4) {
		double temp = stod(argv[3]);
		if (temp <= 0) { cout << "After end position cannot be negative or a zero" << endl; return 0; }
		afterEndPos = temp;
	}
	
	enum OpMode {
		NORMAL,
		PARALLEL_TRAJECTORY
	};
	OpMode opMode = NORMAL;
	if ((argc >= 5) && stoi(argv[4]) == 1) {
		opMode = PARALLEL_TRAJECTORY;
		cout << "Parallel mode activated" << endl;
	}
	else {
		cout << "To 1.0 lerp mode activated" << endl;
	}

	int userR = 100, userG = 100, userB = 100;
	if (argc >= 8) {
		userR = stoi(argv[5]);
		userG = stoi(argv[6]);
		userB = stoi(argv[7]);
		if (userR < 0 || userG < 0 || userB < 0) {
			cout << "RGB value modifications cannot be less that 0" << endl;
			return 0;
		}
	}

	filesystem::path origFilePath = filePath.generic_string() + ".orig";
	error_code copy_error;
	if (!filesystem::exists(origFilePath)) {
		filesystem::copy(filePath, origFilePath, copy_error);
		if (copy_error) {
			cout << "Backup creation failed. Error message: " << copy_error.message() << endl; return 0;
		}
	}

	double interpStart = (startPos < 1 ? startPos : ((startPos - START_OFFSET) * STEP));
	double interpEnd = (afterEndPos <= 1 ? afterEndPos : ((afterEndPos - START_OFFSET) * STEP));
	double firstLerpEnd = interpEnd - STEP * END_LERP_START;

	bool isClamping = (interpEnd < 1.0 ? true : false);

	ifstream inOpenFile(filePath);
	if (!inOpenFile.good()) {
		throw runtime_error("input .cal file cannot be opened");
	}

	vector<tuple<double, double, double, double>> outputValues;
	outputValues.reserve((int)(interpStart / STEP));

	stringstream outputSS;

	string int_tmp;

	while (getline(inOpenFile, int_tmp)) {
		if (int_tmp.empty() || *int_tmp.begin() != '0') {
			outputSS << int_tmp << endl;
			continue;
		}
		size_t endCursorPos = int_tmp.find_first_of(' ');
		double origPos = stod(int_tmp.substr(0, endCursorPos));

		size_t startCursorPos = int_tmp.find_first_not_of(' ', endCursorPos);
		endCursorPos = int_tmp.find_first_of(' ', startCursorPos);
		double origR = stod(int_tmp.substr(startCursorPos, (endCursorPos - startCursorPos)));

		startCursorPos = int_tmp.find_first_not_of(' ', endCursorPos);
		endCursorPos = int_tmp.find_first_of(' ', startCursorPos);
		double origG = stod(int_tmp.substr(startCursorPos, (endCursorPos - startCursorPos)));

		startCursorPos = int_tmp.find_first_not_of(' ', endCursorPos);
		endCursorPos = int_tmp.find_first_of(' ', startCursorPos);
		double origB = stod(int_tmp.substr(startCursorPos, (endCursorPos - startCursorPos)));
			
		if ((interpStart - origPos) < DELTA) {
			break;
		}
		outputValues.emplace_back(origPos, origR, origG, origB);
	}
	inOpenFile.close();

	tuple<double, double, double> lastOrigRGB = { get<1>(outputValues.back()), get<2>(outputValues.back()), get<3>(outputValues.back()) };

	double diff = abs(interpStart - get<1>(outputValues.back()));
	if (opMode == PARALLEL_TRAJECTORY) {
		for (double i = interpStart; i < firstLerpEnd; i += STEP) {
			double parallelValue = i + diff;
			outputValues.emplace_back(i, parallelValue, parallelValue, parallelValue);
		}
	}

	double newInterpStart = (opMode == PARALLEL_TRAJECTORY ? firstLerpEnd : interpStart);
	double newFirst = (opMode == PARALLEL_TRAJECTORY ? newInterpStart + diff : get<0>(lastOrigRGB));
	const double divider = interpEnd - newInterpStart;
	for (double i = newInterpStart; i < LAST_STEP; i += STEP) {
		double tmp = (isClamping ? newFirst : lerp(newFirst, interpEnd, (i - newInterpStart) / divider));
		outputValues.emplace_back(i, tmp, tmp, tmp);
	}

	// use chosen lerp start point to calculate color offsets
	// calculate max channel
	double tpp = max(max(userR, userG), max(userR, userB));
	// normalize channel offset values and calculate new values
	double tmpR = get<0>(lastOrigRGB) * ((double)userR / tpp);
	double tmpG = get<1>(lastOrigRGB) * ((double)userG / tpp);
	double tmpB = get<2>(lastOrigRGB) * ((double)userB / tpp);
	// calculate final "static" offsets
	double modifiedMaxChannel = max(max(tmpR, tmpG), max(tmpR, tmpB));
	double rOffset = modifiedMaxChannel - tmpR;
	double gOffset = modifiedMaxChannel - tmpG;
	double bOffset = modifiedMaxChannel - tmpB;

	// tries to fix lost brightness due to changing rgb values, thus inherently losing some brightness
	// mb a bit crude, but better than nothing
	// ideally it should be based on our perception of light, and should be further researched
	//double brightnessOffset = max(max(rOffset, gOffset), max(rOffset, bOffset)) / 3;
	//const double brightnessOffset = (get<0>(origRGB) + get<1>(origRGB) + get<2>(origRGB)) / 3 - (tmpR + tmpG + tmpB) / 3;
	const double brightnessOffset = 0.0;

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

	for (const auto& values : outputValues) {
		double r = min(1.0, max(0.0, get<1>(values) - rOffset));
		double g = min(1.0, max(0.0, get<2>(values) - gOffset));
		double b = min(1.0, max(0.0, get<3>(values) - bOffset));

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

	ostringstream finalClampedR;
	finalClampedR << setprecision(15) << std::left << setfill('0') << setw(17) << max(1.0, get<1>(outputValues.back()) - rOffset);
	ostringstream finalClampedG;
	finalClampedG << setprecision(15) << std::left << setfill('0') << setw(17) << min(1.0, get<2>(outputValues.back()) - gOffset);
	ostringstream finalClampedB;
	finalClampedB << setprecision(15) << std::left << setfill('0') << setw(17) << min(1.0, get<3>(outputValues.back()) - bOffset);

	//lut1dOutFile << (isClamping ? finalClampedR.str() : "1.000000000000000") << ',';
	//lut1dOutFile << (isClamping ? finalClampedG.str() : "1.000000000000000") << ',';
	//lut1dOutFile << (isClamping ? finalClampedB.str() : "1.000000000000000");

	//outOpenFile << "1.00000000000000 ";
	//outOpenFile << (isClamping ? finalClampedR.str() : "1.000000000000000") << ' ';
	//outOpenFile << (isClamping ? finalClampedG.str() : "1.000000000000000") << ' ';
	//outOpenFile << (isClamping ? finalClampedB.str() : "1.000000000000000");
	//outOpenFile << endl;
	outOpenFile << "END_DATA";
}