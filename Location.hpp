#pragma once
#include <iostream>
#include <string>
#include <vector>

class Location
{
private:
	std::string path;
	std::vector<std::string> methods;
	std::string root;
	std::string index;
	bool autoindex;
	int redirectionCode;
	std::string redirection;

public:
	// Конструкторы и деструкторы
	Location();
	~Location();
	Location(const Location &other);
	Location &operator=(const Location &other);

	// Сеттеры (используешь ТЫ в Роли 2, когда парсишь файл)
	void setPath(const std::string &p);
	void addMethod(const std::string &m);
	void setRoot(const std::string &r);
	void setIndex(const std::string &i);
	void setRedirection(const std::string &r);
	void setRedirectionCode(int code);
	void setAutoIndex(const std::string &a);

	// Геттеры (использует РОЛЬ 3, когда генерирует ответ)
	std::string getPath() const;
	const std::vector<std::string> &getMethods() const;
	std::string getRoot() const;
	std::string getIndex() const;
	bool getAutoindex() const;
	std::string getRedirection() const;
	int getRedirectionCode() const;
};
