#ifndef CHESS_H
#define CHESS_H

typedef enum {
    CHESS_PAWN = 0,
    CHESS_BISHOP = 1,
    CHESS_ROOK = 2,
    CHESS_KNIGHT = 3,
    CHESS_QUEEN = 4,
    CHESS_KING = 5
} chess_type_t;
typedef enum {
    CHESS_NORMAL = 0,
    CHESS_CHECK = 1,
    CHESS_CHECKMATE  = 2,
    CHESS_STALEMATE = 3

} chess_status_t;

typedef signed char chess_value_t;

#define CHESS_TEAM(id) (!!(id & (1 << 3)))
#define CHESS_TYPE(id) ((chess_type_t)(id & 7))
#define CHESS_ID(team, type) ((team ? (1 << 3) : (0 << 3)) | (int)type)
#define CHESS_NONE ((chess_value_t)-1)

typedef struct {
    chess_value_t board[64];
    chess_value_t kings[2];
    chess_value_t en_passant_targets[16];
    chess_value_t turn;
    chess_value_t no_castle[2];
} chess_game_t;

#ifdef __cplusplus
extern "C" {
#endif
/// @brief Initializes a new chess game
/// @param out_game The structure holding the game
void chess_init(chess_game_t* out_game);
/// @brief Moves a piece from one position to another
/// @param game the chess game  
/// @param index_from The index to move from
/// @param index_to The index to move to.
/// @return The index of the capture victim if successful. -1 if no capture. -2 on illegal move or invalid arguments
chess_value_t chess_move(chess_game_t* game, chess_value_t index_from, chess_value_t index_to);
/// @brief Computes the available moves for a specified piece on the board
/// @param game The chess game
/// @param index The index on the board for the piece to compute
/// @param out_moves The moves array to write to (should be at least 64 length)
/// @return The count of moves written
chess_value_t chess_compute_moves(const chess_game_t* game, chess_value_t index, chess_value_t* out_moves);
/// @brief Indicates whether an array of move destinations contains the specified index
/// @param moves The moves array
/// @param moves_size The size of the moves array
/// @param index The index to compare
/// @return Nonzero if the move is present, otherwise zero
chess_value_t chess_contains_move(const chess_value_t* moves, chess_value_t moves_size, chess_value_t index);
/// @brief Promotes a pawn that has reached the end of the board
/// @param game The chess game
/// @param index The index of the pawn
/// @param new_type The new chess piece type
/// @return Nonzero if the promotion was successful, otherwise zero
chess_value_t chess_promote_pawn(chess_game_t* game, chess_value_t index, chess_type_t new_type);
/// @brief Indicates the status of the game
/// @param game The game
/// @param team The team to check (0 or 1)
/// @return Whether the game is in a normal state, a check state, or whether mate has occurred
chess_status_t chess_status(const chess_game_t* game, chess_value_t team);
/// @brief Indicates which player's turn it is
/// @param game The game
/// @return 0 or 1 depending on the team that is up
chess_value_t chess_turn(const chess_game_t* game);
/// @brief Gets the piece id at the board index
/// @param game The game
/// @param index The index of the piece to retrieve
/// @return the id or -1 if invalid or no piece present
chess_value_t chess_index_to_id(const chess_game_t* game, chess_value_t index);
/// @brief Returns the canonical name for the board index, such as "b7"
/// @param index The board index
/// @param out_buffer A string buffer of at least 3 characters
/// @return Nonzero if the operation was successful, otherwise zero if invalid argument
chess_value_t chess_index_name(int index,char* out_buffer);
#ifdef __cplusplus
}
#endif

#endif // CHESS_H