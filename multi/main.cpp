#include "./engine/engine.hpp"
#include "./server/server.hpp"
#include <iostream>

int main()
{
	try
	{
		// 1. Создаем сервер (допустим, ты уже написал конструктор, 
		// который делает socket(), bind() на порт 8080 и listen())
		Server* my_server = new Server(8080, "127.0.0.1");

		// 2. Создаем движок
		Engine engine;

		// 3. Подключаем сервер к движку
		engine.addServer(my_server);

		std::cout << "[Main]: Server is running on port 8080..." << std::endl;

		// 4. Запускаем бесконечный цикл
		engine.run();
	}
	catch (const std::exception& e)
	{
		std::cerr << "Fatal error: " << e.what() << std::endl;
		return (1);
	}

	return (0);
}