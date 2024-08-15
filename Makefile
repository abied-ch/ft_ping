NAME = ft_ping

OBJ_DIR = obj

SRC_DIR = src/


SRCS = 	${SRC_DIR}main.c \

OBJS = $(patsubst $(SRC_DIR)%.c, $(OBJ_DIR)/%.o, $(SRCS))

CC = cc

CFLAGS = -Wall -Wextra -Werror -Iinc -g 

LDFLAGS = -L ./libft -lm -lft

RM = rm -rf

all: $(OBJ_DIR) $(NAME)

$(NAME): $(OBJS) $(LIBFT_OBJS)
	@$(MAKE) -C ./libft --no-print-directory
	$(CC) $(CFLAGS) $(OBJS) $(LIBFT_OBJS) -o $(NAME) $(LDFLAGS)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)
	mkdir -p $(OBJ_DIR)$(STANDARD)$(RAY_DIR)
	mkdir -p $(OBJ_DIR)$(STANDARD)$(MAP_DIR)

$(OBJ_DIR)/%.o: src/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(MAKE) -C ./libft --no-print-directory fclean
	$(RM) $(OBJ_DIR)

fclean: clean
	$(RM) $(NAME) $(TEST_NAME)

re: fclean all

.PHONY: all clean fclean re
