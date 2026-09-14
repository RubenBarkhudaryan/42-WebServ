#include "../../include/http/CgiProcess.hpp"

#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>

CgiProcess::CgiProcess(pid_t pid, int stdinFd, int stdoutFd, const std::string &input) :
	pid(pid),
	stdinFd(stdinFd),
	stdoutFd(stdoutFd),
	input(input),
	inputOffset(0),
	reaped(false),
	exitStatus(0),
	clientFd(-1)
{
	fcntl(this->stdinFd, F_SETFL, O_NONBLOCK);
	fcntl(this->stdinFd, F_SETFD, FD_CLOEXEC);
	fcntl(this->stdoutFd, F_SETFL, O_NONBLOCK);
	fcntl(this->stdoutFd, F_SETFD, FD_CLOEXEC);

	if (this->input.empty())
	{
		close(this->stdinFd);
		this->stdinFd = -1;
	}
}

CgiProcess::~CgiProcess()
{
	if (this->stdinFd != -1)
		close(this->stdinFd);
	if (this->stdoutFd != -1)
		close(this->stdoutFd);
}

pid_t	CgiProcess::getPid() const
{
	return this->pid;
}

int	CgiProcess::getStdinFd() const
{
	return this->stdinFd;
}

int	CgiProcess::getStdoutFd() const
{
	return this->stdoutFd;
}

int	CgiProcess::getClientFd() const
{
	return this->clientFd;
}

void	CgiProcess::setClientFd(int fd)
{
	this->clientFd = fd;
}

bool	CgiProcess::isInputOpen() const
{
	return this->stdinFd != -1;
}

bool	CgiProcess::isOutputOpen() const
{
	return this->stdoutFd != -1;
}

void	CgiProcess::handleWritable()
{
	if (this->stdinFd == -1)
		return ;

	ssize_t	count = write(this->stdinFd, this->input.data() + this->inputOffset,
		this->input.size() - this->inputOffset);

	if (count <= 0)
	{
		close(this->stdinFd);
		this->stdinFd = -1;
		return ;
	}

	this->inputOffset += static_cast<std::string::size_type>(count);
	if (this->inputOffset >= this->input.size())
	{
		close(this->stdinFd);
		this->stdinFd = -1;
	}
}

void	CgiProcess::handleReadable()
{
	if (this->stdoutFd == -1)
		return ;

	char	buffer[4096];
	ssize_t	count = read(this->stdoutFd, buffer, sizeof(buffer));

	if (count <= 0)
	{
		close(this->stdoutFd);
		this->stdoutFd = -1;
		return ;
	}

	this->output.append(buffer, count);
}

bool	CgiProcess::tryReap()
{
	if (this->reaped)
		return true;

	int		status = 0;
	pid_t	result = waitpid(this->pid, &status, WNOHANG);

	if (result != this->pid)
		return false;

	this->exitStatus = status;
	this->reaped = true;
	return true;
}

bool	CgiProcess::isReaped() const
{
	return this->reaped;
}

int	CgiProcess::getExitStatus() const
{
	return this->exitStatus;
}

const std::string	&CgiProcess::getOutput() const
{
	return this->output;
}
