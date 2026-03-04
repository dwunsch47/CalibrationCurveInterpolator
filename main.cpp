#include <iostream>
#include <filesystem>
#include <string>
#include <fstream>
#include <sstream>
#include <cmath>

using namespace std;

const int START_OFFSET = 14; // actual numerical data start on line 14
const int LAST_LINE = 1037; // 1038 line or 1.0 is always final
const double STEP = 1.0 / 1023.0;
const double DELTA = 0.0001;


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
		startPos = startPos / 254.0;
		cout << startPos << endl;
	}
	if (startPos < 0) { cout << "Start position cannot be a negative number" << endl; return 0; }
	else if (startPos == 1) { cout << "Start position cannot be \"1\" or \"1.0\"" << endl; return 0; }

	double afterEndPos = 1.0;
	if (argc == 4) {
		double temp = stod(argv[3]);
		if (temp <= 0) { cout << "After end position cannot be negative or a zero" << endl; return 0; }
		afterEndPos = temp;
	}
	
	enum OpMode {
		NORMAL,
		PARALLEL_TRAJECTORY
	};
	OpMode opMode = NORMAL;
	if ((argc == 5) && stoi(argv[4]) == 1) {
		opMode = PARALLEL_TRAJECTORY;
	}

	bool isFullRange = false;
	int userR = 50, userG = 50, userB = 50;
	if (argc == 8) {
		userR = stoi(argv[5]);
		if (userR > 50) isFullRange = true;
		userG = stoi(argv[6]);
		if (userG > 50) isFullRange = true;
		userB = stoi(argv[7]);
		if (userB > 50) isFullRange = true;
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
	double firstLerpEnd = interpEnd - STEP * 100;

	ifstream inOpenFile(filePath);
	if (!inOpenFile.good()) {
		throw runtime_error("input .cal file cannot be opened");
	}

	tuple<double, double, double> origRGB;

	stringstream outputSS;

	string tmp;
	size_t startCursorPos, endCursorPos = 0;

	while (getline(inOpenFile, tmp)) {
		if (tmp.empty() || *tmp.begin() != '0') {
			outputSS << tmp << endl;
			continue;
		}
		endCursorPos = tmp.find_first_of(' ');
		if ((interpStart - stod(tmp.substr(0, endCursorPos))) < DELTA) {
			startCursorPos = tmp.find_first_not_of(' ', endCursorPos);
			endCursorPos = tmp.find_first_of(' ', startCursorPos);
			get<0>(origRGB) = stod(tmp.substr(startCursorPos, (endCursorPos - startCursorPos)));

			startCursorPos = tmp.find_first_not_of(' ', endCursorPos);
			endCursorPos = tmp.find_first_of(' ', startCursorPos);
			get<1>(origRGB) = stod(tmp.substr(startCursorPos, (endCursorPos - startCursorPos)));

			startCursorPos = tmp.find_first_not_of(' ', endCursorPos);
			endCursorPos = tmp.find_first_of(' ', startCursorPos);
			get<2>(origRGB) = stod(tmp.substr(startCursorPos, (endCursorPos - startCursorPos)));
			
			break;
		}
		outputSS << tmp << endl;
	}
	inOpenFile.close();

	double diff = abs(interpStart - get<0>(origRGB));
	double parallelValue;
	if (opMode == PARALLEL_TRAJECTORY) {
		for (double i = interpStart; i < (firstLerpEnd - STEP); i += STEP) {
			parallelValue = i + diff;
			outputSS << setprecision(14) << std::left << setfill('0') << setw(16) << i << ' ';
			outputSS << setprecision(15);
			outputSS << std::left << setfill('0') << setw(17) << parallelValue << ' ';
			outputSS << std::left << setfill('0') << setw(17) << parallelValue << ' ';
			outputSS << std::left << setfill('0') << setw(17) << parallelValue;
			outputSS << endl;
		}
	}

	double newInterpStart = (opMode == PARALLEL_TRAJECTORY ? firstLerpEnd : interpStart);
	double newFirst = (opMode == PARALLEL_TRAJECTORY ? newInterpStart + diff : get<0>(origRGB));
	for (double i = newInterpStart; i < (interpEnd - STEP); i += STEP) {
		outputSS << setprecision(14) << std::left << setfill('0') << setw(16) << i << ' ';
		outputSS << setprecision(15);
		outputSS << std::left << setfill('0') << setw(17) << lerp(newFirst, interpEnd, (i - newInterpStart) / (interpEnd - newInterpStart)) << ' ';
		outputSS << std::left << setfill('0') << setw(17) << lerp(newFirst, interpEnd, (i - newInterpStart) / (interpEnd - newInterpStart)) << ' ';
		outputSS << std::left << setfill('0') << setw(17) << lerp(newFirst, interpEnd, (i - newInterpStart) / (interpEnd - newInterpStart));
		outputSS << endl;
	}

	outputSS << "1.00000000000000 1.000000000000000 1.000000000000000 1.000000000000000" << endl;
	outputSS << "END_DATA";


	filesystem::path newFilePath = filePath.replace_filename(filePath.stem().generic_string() + "_interpolated" + filePath.extension().generic_string());

	ofstream outOpenFile(newFilePath);
	if (!outOpenFile.good()) {
		throw runtime_error("ouput .cal file cannot be opened");
	}

	double pos;
	double r, g, b;

	outOpenFile << fixed;

	while (getline(outputSS, tmp)) {
		if (tmp.empty() || *tmp.begin() != '0') {
			outOpenFile << tmp << endl;
			continue;
		}
		endCursorPos = tmp.find_first_of(' ');
		pos = stod(tmp.substr(0, endCursorPos));

		startCursorPos = tmp.find_first_not_of(' ', endCursorPos);
		endCursorPos = tmp.find_first_of(' ', startCursorPos);
		r = stod(tmp.substr(startCursorPos, (endCursorPos - startCursorPos))) * (double)userR * (isFullRange ? 0.01 : 0.02);

		startCursorPos = tmp.find_first_not_of(' ', endCursorPos);
		endCursorPos = tmp.find_first_of(' ', startCursorPos);
		g = stod(tmp.substr(startCursorPos, (endCursorPos - startCursorPos))) * (double)userG * (isFullRange ? 0.01 : 0.02);

		startCursorPos = tmp.find_first_not_of(' ', endCursorPos);
		endCursorPos = tmp.find_first_of(' ', startCursorPos);
		b = stod(tmp.substr(startCursorPos, (endCursorPos - startCursorPos))) * (double)userB * (isFullRange ? 0.01 : 0.02);

		outOpenFile << setprecision(14) << std::left << setfill('0') << setw(16) << pos << ' ';
		outOpenFile << setprecision(15);
		outOpenFile << std::left << setfill('0') << setw(17) << r << ' ';
		outOpenFile << std::left << setfill('0') << setw(17) << g << ' ';
		outOpenFile << std::left << setfill('0') << setw(17) << b;
		outOpenFile << endl;
	}
}