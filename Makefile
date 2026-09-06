NAME		=	webserv

CXX			=	c++
CXXFLAGS	=	-std=c++98 -Wall -Wextra -Werror

SRCS		=	./sockets/engine/engine.cpp\
				./sockets/server/server.cpp\
				./sockets/client/client.cpp\
				./parser/src/config/ConfigParser.cpp\
				./parser/src/config/ConfigParserBlocks.cpp\
				./parser/src/config/ServerConfig.cpp\
				./parser/src/debug/ServerPrinter.cpp\
				./parser/src/http/HttpRequest.cpp\
				./parser/src/http/HttpResponse.cpp\
				./parser/src/http/RequestHandler.cpp\
				./parser/src/model/Location.cpp\
				./main.cpp

OBJS		=	$(SRCS:%.cpp=%.o)

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)

%.o : %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJS)

fclean: clean
	rm -rf $(NAME)

re: fclean all

.PHONY: all clean fclean re