#ifndef CHESS_H
#define CHESS_H

typedef enum {
    CHESS_PAWN = 0,
    CHESS_BISHOP = 1,
    CHESS_ROOK = 2,
    CHESS_KNIGHT = 3,
    CHESS_QUEEN = 4,
    CHESS_KING = 5
} chess_piece_type_t;
typedef enum {
    CHESS_NORMAL = 0,
    CHESS_CHECK = 1,
    CHESS_MATE  = 2
} chess_status_t;

#define CHESS_TEAM(id) (!!(id & (1 << 3)))
#define CHESS_TYPE(id) (id & 7)
#define CHESS_ID(team, type) ((team ? (1 << 3) : (0 << 3)) | (int)type)
#define CHESS_NONE (-1)

typedef struct {
    signed char board[64];
    signed char kings[2];
    signed char turn;
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
/// @return Nonzero if it was the right turn, and the move was valid. Otherwise zero
signed char chess_move(chess_game_t* game, signed char index_from, signed char index_to);
/// @brief Computes the available moves for a specified piece on the board
/// @param game The chess game
/// @param index The index on the board for the piece to compute
/// @param out_moves The moves array to write to (should be at least 64 length)
/// @return The count of moves written
signed char chess_compute_moves(const chess_game_t* game, signed char index, signed char* out_moves);
/// @brief Indicates whether an array of move destinations contains the specified index
/// @param moves The moves array
/// @param moves_size The size of the moves array
/// @param index The index to compare
/// @return Nonzero if the move is present, otherwise zero
signed char chess_contains_move(const signed char* moves, signed char moves_size, signed char index);
/// @brief Promotes a pawn that has reached the end of the board
/// @param game The chess game
/// @param index The index of the pawn
/// @param new_type The new chess piece type
/// @return Nonzero if the promotion was successful, otherwise zero
signed char chess_promote_pawn(chess_game_t* game, signed char index, signed char new_type);
/// @brief Indicates the status of the game
/// @param game The game
/// @param team The team to check
/// @return Whether the game is in a normal state, a check state, or whether mate has occurred
chess_status_t chess_status(const chess_game_t* game, signed char team);
/// @brief Indicates which player's turn it is
/// @param game The game
/// @return 0 or 1 depending on the team that is up
signed char chess_turn(const chess_game_t* game);
/// @brief Gets the piece id at the board index
/// @param game The game
/// @param index The index of the piece to retrieve
/// @return the id or -1 if invalid or no piece present
signed char chess_index_to_id(const chess_game_t* game, signed char index);
#ifdef __cplusplus
}
#endif

#endif // CHESS_H