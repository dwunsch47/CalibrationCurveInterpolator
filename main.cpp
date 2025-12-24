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
	if ((argc < 3) || (argc > 4)) {
		cout << "Wrong format. Use \"./cci.exe \"path to file or filename if file is in current folder\" "
			<< "\"start position of interpolation (line number or float, or curve pos from DisplayCal's curve viewer)\" "
			<< "\"(optional(if nothing, assusmes \"1.0\" or \"1037\n)) after end position of interpolation (line number or float)\"" << endl;
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

	filesystem::path origFilePath = filePath.generic_string() + ".orig";
	error_code copy_error;
	if (!filesystem::exists(origFilePath)) {
		filesystem::copy(filePath, origFilePath, copy_error);
		if (copy_error) {
			cout << "Backup creation failed. Error message: " << copy_error.message() << endl; return 0;
		}
	}

	double itStart = (startPos < 1 ? startPos : ((startPos - START_OFFSET) * STEP));
	double itEnd = (afterEndPos <= 1 ? afterEndPos : ((afterEndPos - START_OFFSET) * STEP));

	ifstream inOpenFile(filePath);
	if (!inOpenFile.good()) {
		throw runtime_error(".cal file cannot be opened");
	}

	tuple<double, double, double> origRGB;

	string tmp;
	size_t startCursorPos, endCursorPos = 0;
	stringstream origValues;

	while (getline(inOpenFile, tmp)) {
		if (tmp.empty() || *tmp.begin() != '0') {
			origValues << tmp << endl;
			continue;
		}
		endCursorPos = tmp.find_first_of(' ');
		if ((itStart - stod(tmp.substr(0, endCursorPos))) < DELTA) {
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
		origValues << tmp << endl;
	}
	inOpenFile.close();

	stringstream lerpValues;

	for (double i = itStart; i < (itEnd - STEP); i += STEP) {

		lerpValues << setprecision(14) << std::left << setfill('0') << setw(16) << i << ' ';
		lerpValues << setprecision(15);
		lerpValues << std::left << setfill('0') << setw(17) << lerp(get<0>(origRGB), itEnd, (i - itStart) / (itEnd - itStart)) << ' ';
		lerpValues << std::left << setfill('0') << setw(17) << lerp(get<1>(origRGB), itEnd, (i - itStart) / (itEnd - itStart)) << ' ';
		lerpValues << std::left << setfill('0') << setw(17) << lerp(get<2>(origRGB), itEnd, (i - itStart) / (itEnd - itStart));
		lerpValues << endl;
	}


	filesystem::path newFilePath = filePath.replace_filename(filePath.stem().generic_string() + "_interpolated" + filePath.extension().generic_string());

	ofstream outOpenFile(newFilePath);
	if (!outOpenFile.good()) {
		throw runtime_error(".cal file cannot be opened");
	}

	outOpenFile << origValues.rdbuf() << lerpValues.rdbuf();
	outOpenFile << "1.00000000000000 1.000000000000000 1.000000000000000 1.000000000000000" << endl;
	outOpenFile << "END_DATA" << endl;
}