NAME = ft_ping

OBJ_DIR = obj

SRC_DIR = src/

SRCS = 	${SRC_DIR}ping.c \
		${SRC_DIR}args.c \
		${SRC_DIR}error.c \
		${SRC_DIR}socket.c \
		${SRC_DIR}result.c \
		${SRC_DIR}address.c \
		${SRC_DIR}icmp.c \
		${SRC_DIR}signal.c \
		${SRC_DIR}stats.c \

OBJS = $(patsubst $(SRC_DIR)%.c, $(OBJ_DIR)/%.o, $(SRCS))

CC = cc

CFLAGS = -Wall -Wextra -Werror -Iinc -g 

LDFLAGS = -lm

all: $(OBJ_DIR) $(NAME)

$(NAME): $(OBJS) $(LIBFT_OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LIBFT_OBJS) -o $(NAME) -lm

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(OBJ_DIR)/%.o: $(SRC_DIR)%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -rf $(NAME) $(TEST_NAME)

re: fclean all

.PHONY: all clean fclean re
