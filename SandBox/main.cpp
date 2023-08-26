#include <filesystem>
#include <fstream>
#include <thread>
#include <mutex>

// Malicious function that can print to ofs without going through the lock
void malicious(std::ofstream& ofs)
{
	std::string filename = "F:\\info.txt";
	ofs = std::ofstream(filename, std::ofstream::out);
	if (!ofs.is_open())
	{
		ofs.open(filename);
	}
	for (int i = 0; i <= 100; ++i)
	{
		ofs << "This is a malicious act " << i << '\n'; // Writing directly to the file
		std::string msg = "From t1: " + std::to_string(i) + '\n';
		ofs.write(msg.data(), msg.length()); // Writing directly to the file
	}
}

struct LogFile
{
	LogFile(std::string_view filename_) :
		mtx(),
		ofs(),
		filename(filename_)
	{
		// Constructor initializes members but does not open the file
	}

	void write_into_file_V2(std::string_view str)
	{

		std::call_once(this->openfile_flag, [&]() {

			// Opening the file if not already open
			this->ofs = std::ofstream(this->filename, std::ofstream::out);

			});

		std::lock_guard<std::mutex> lock(this->mtx); // Locking to ensure thread safety
		this->ofs << str << '\n'; // Writing the string to the file
	}

	void write_into_file(const std::string& str)
	{
		// This will be called only once and only by one thread as we only need to open the file once
		std::call_once(this->openfile_flag, [&]() {

			// Opening the file if not already open
			this->ofs = std::ofstream(this->filename, std::ofstream::out);
			});

		std::lock_guard<std::mutex> lock(this->mtx); // Locking to ensure thread safety
		this->ofs.write(str.data(), str.length()); // Writing the string to the file
	}

	// WARNING: Returning a reference to ofs allows direct access to the stream
	// without going through the lock, potentially leading to race conditions
	// Exposing Internal Resources
	std::ofstream& get_ofstream()
	{
		return this->ofs;
	}

	// This function takes a user-provided function and applies it to the ofstream
	// This is risky as it allows the user to bypass the mutex and directly manipulate the file
	// Bypassing Mutexes
	void process_ofstream(void (*func_ptr)(std::ofstream& ofs))
	{
		func_ptr(this->ofs); // Calling the user-provided function with the ofstream
	}

	~LogFile()
	{
		this->ofs.close(); // Closing the file when the LogFile object is destroyed
	}

private:
	std::mutex mtx;					// Mutex for synchronizing access to the file
	std::once_flag openfile_flag;   // We only need to open a file once during execution of a program
	std::ofstream ofs;				// The output file stream (the resource)
	std::string filename;			// The name of the file
};

void function_1(LogFile& logFile)
{
	for (int i = 0; i > -100; --i)
	{
		std::string msg = "From t1: " + std::to_string(i) + '\n'; // Message to be written
		logFile.write_into_file(msg);    // Writing using method 1
		logFile.write_into_file_V2(msg); // Writing using method 2
	}
}

void function_2()
{
	for (int i = 400; i <= 500; i += 1)
	{
		std::cout << "From t2: " << i << '\n'; // Printing to standard output
	}
}

int main()
{
	LogFile logFile("C:\\test.txt");
	std::jthread t1(function_1, std::ref(logFile));
	std::jthread t2(function_2);
	logFile.process_ofstream(malicious); // Invoking the malicious function
	for (int i = 0; i < 100; ++i)
	{
		std::string msg = "From main: " + std::to_string(i) + '\n'; // Message to be written
		logFile.write_into_file(msg);    // Writing using method 1
		logFile.write_into_file_V2(msg); // Writing using method 2
	}
}
