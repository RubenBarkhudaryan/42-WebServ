#pragma once

#include <string>
#include <sys/types.h>

/*
** Tracks one forked CGI child so its stdin/stdout pipes can be driven
** incrementally from the single poll() loop instead of blocking on
** write()/read()/waitpid(). Engine owns the poll-driven lifecycle;
** RequestHandler owns forking the child and interpreting its output.
*/
class CgiProcess
{
private:
	pid_t					pid;
	int						stdinFd;
	int						stdoutFd;
	std::string				input;
	std::string::size_type	inputOffset;
	std::string				output;
	bool					reaped;
	int						exitStatus;
	int						clientFd;

	CgiProcess(const CgiProcess &other);
	CgiProcess &operator=(const CgiProcess &other);

public:
	CgiProcess(pid_t pid, int stdinFd, int stdoutFd, const std::string &input);
	~CgiProcess();

	pid_t	getPid() const;
	int		getStdinFd() const;
	int		getStdoutFd() const;
	int		getClientFd() const;
	void	setClientFd(int fd);

	bool	isInputOpen() const;
	bool	isOutputOpen() const;

	void	handleWritable();
	void	handleReadable();

	bool	tryReap();
	bool	isReaped() const;
	int		getExitStatus() const;
	const std::string	&getOutput() const;
};
