NAME        := webserv

CXX         := c++
CXXFLAGS    := -Wall -Wextra -Werror -std=c++98 -MMD -MP

SRC_DIRS    := src
SRCS        := $(shell find $(SRC_DIRS) -type f -name '*.cpp' | sort)
OBJS        := $(SRCS:%.cpp=obj/%.o)
DEPS        := $(OBJS:.o=.d)

INCLUDES    := -Iinclude

ifeq ($(strip $(SRCS)),)
$(error No source files found. Add .cpp files to build $(NAME).)
endif

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $@

obj/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -rf obj

fclean: clean
	rm -f $(NAME)

re: fclean all

-include $(DEPS)

.PHONY: all clean fclean re
