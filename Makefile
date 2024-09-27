NAME = ft_ping

OBJ_DIR = obj
SRC_DIR = src
INC_DIR = inc

SRCS = ping.c args.c error.c socket.c result.c address.c icmp.c signal.c stats.c
OBJS = $(addprefix $(OBJ_DIR)/, $(SRCS:.c=.o))

CC = cc
CFLAGS = -Wall -Wextra -Werror -I$(INC_DIR) -g
LDFLAGS = -lm

all: $(NAME)

$(NAME): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(NAME) $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(INC_DIR)/ping.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re
