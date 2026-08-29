NAME		= webserv
CXX			= c++
CXXFLAGS	= -Wall -Wextra -Werror -std=c++98
INCLUDES	= -Iincludes
OBJ_DIR		= objs
SRCS		= sources/StringUtils.cpp \
			sources/NetUtils.cpp \
			sources/FsUtils.cpp \
			sources/HttpUtils.cpp \
			sources/HttpRequest.cpp \
			sources/HttpResponseBuilder.cpp \
			sources/Client.cpp \
			sources/Router.cpp \
			sources/RequestHandler.cpp \
			sources/ConfigParser.cpp \
			sources/ConfigStructs.cpp \
			sources/ListenTable.cpp \
			sources/Server.cpp \
			sources/ServerCgi.cpp \
			sources/main.cpp

OBJS		= $(SRCS:sources/%.cpp=$(OBJ_DIR)/%.o)

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)

$(OBJ_DIR)/%.o: sources/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re
