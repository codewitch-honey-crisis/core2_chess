#include "chess.h"
#include <memory.h>
#ifndef NULL
#define NULL 0
#endif
static void move_until_obstacle(signed char (*index_fn)(signed char team, signed char index), signed char team, signed char index, const signed char* game_board, signed char* out_moves, signed char* out_size) {
    signed char i = index;
    while (1) {
        i = index_fn(team, i);
        if (i != -1) {
            if (game_board[i] != -1) {
                if (CHESS_TEAM(game_board[i]) == team) {
                    return;
                }
                out_moves[(*out_size)++] = i;
                return;
            } else {
                out_moves[(*out_size)++] = i;
            }
        } else {
            break;
        }
    }
}
static signed char index_advance(signed char team, signed char index) {
    if (index == -1) return -1;
    if (team == 0) {
        index += 8;
    } else if (team == 1) {
        index -= 8;
    }
    if (index < 0 || index > 63) return -1;
    return index;
}
static signed char index_advance_left(signed char team, signed char index) {
    if (index == -1) return -1;
    const signed char x = index % 8;
    if (team == 0) {
        if (x == 7 || index + 8 > 63) {
            return -1;
        }
        index += 9;
    } else if (team == 1) {
        if (x == 0 || index - 9 < 0) {
            return -1;
        }
        index -= 9;
    }
    if (index < 0 || index > 63) return -1;
    return index;
}
static signed char index_advance_right(signed char team, signed char index) {
    if (index == -1) return -1;
    const signed char x = index % 8;
    if (team == 0) {
        if (x == 0 || index + 7 > 63) {
            return -1;
        }
        index += 7;
    } else if (team == 1) {
        if (x == 7 || index - 7 < 0) {
            return -1;
        }
        index -= 7;
    }
    if (index < 0 || index > 63) return -1;
    return index;
}
static signed char index_retreat_left(signed char team, signed char index) {
    if (index == -1) return -1;
    const signed char x = index % 8;
    if (team == 0) {
        if (x == 7 || index - 7 < 0) {
            return -1;
        }
        index -= 7;
    } else if (team == 1) {
        if (x == 0 || index + 7 > 63) {
            return -1;
        }
        index += 7;
    }
    if (index < 0 || index > 63) return -1;
    return index;
}
static signed char index_retreat_right(signed char team, signed char index) {
    if (index == -1) return -1;
    const signed char x = index % 8;
    if (team == 0) {
        if (x == 0 || index - 9 < 0) {
            return -1;
        }
        index -= 9;
    } else if (team == 1) {
        if (x == 7 || index + 9 > 63) {
            return -1;
        }
        index += 9;
    }
    if (index < 0 || index > 63) return -1;
    return index;
}
static signed char index_retreat(signed char team, signed char index) {
    if (index == -1) return -1;
    if (team == 1) {
        index += 8;
    } else if (team == 0) {
        index -= 8;
    }
    if (index < 0 || index > 63) return -1;
    return index;
}
static signed char index_left(signed char team, signed char index) {
    if (index == -1) return -1;
    const signed char x = index % 8;
    if (team == 0) {
        if (x == 7) {
            return -1;
        }
        index += 1;
    } else if (team == 1) {
        if (x == 0) {
            return -1;
        }
        index -= 1;
    }
    if (index < 0 || index > 63) return -1;
    return index;
}
static signed char index_right(signed char team, signed char index) {
    if (index == -1) return -1;
    const signed char x = index % 8;
    if (team == 1) {
        if (x == 7) {
            return -1;
        }
        index += 1;
    } else if (team == 0) {
        if (x == 0) {
            return -1;
        }
        index -= 1;
    }
    if (index < 0 || index > 63) return -1;
    return index;
}
static signed char compute_moves(signed char index, signed char* out_moves, const signed char* game_board) {
    const signed char id = game_board[index];
    if (id == -1) {
        return 0;
    }
    const signed char type = CHESS_TYPE(id);
    const signed char team = CHESS_TEAM(id);
    signed char result = 0;
    switch (type) {
        case CHESS_PAWN:
            if ((team == 0 && index < 16) || (team == 1 && index > (64 - 16))) {  // the pawn is on its first move
                signed char tmp = index_advance(team, index);
                if(tmp!=-1) {
                    if(game_board[tmp]==-1) {
                        out_moves[result++] = tmp;
                    }
                    signed char attack = index_advance_left(team, index);
                    if (attack != -1 && game_board[attack] != -1 && CHESS_TEAM(game_board[attack]) != team) {
                        out_moves[result++] = attack;
                    }
                    attack = index_advance_right(team, index);
                    if (attack != -1 && game_board[attack] != -1 && CHESS_TEAM(game_board[attack]) != team) {
                        out_moves[result++] = attack;
                    }
                    if(game_board[tmp]==-1) {
                        const signed char tmp2 = index_advance(team, tmp);
                        if(tmp2!=-1 && game_board[tmp2]==-1) {
                            out_moves[result++] = tmp2;
                        }    
                    }
                    attack = index_advance_left(team, tmp);
                    if (attack != -1 && game_board[attack] != -1 && CHESS_TEAM(game_board[attack]) != team) {
                        out_moves[result++] = attack;
                    }
                    attack = index_advance_right(team, tmp);
                    if (attack != -1 && game_board[attack] != -1 && CHESS_TEAM(game_board[attack]) != team) {
                        out_moves[result++] = attack;
                    }
                }
            } else {
                signed char tmp = index_advance(team, index);
                if (tmp != -1) {
                    if(game_board[tmp]==-1) {
                        out_moves[result++] = tmp;
                    }
                    signed char attack = index_advance_left(team, index);
                    if (attack != -1 && game_board[attack] != -1 && CHESS_TEAM(game_board[attack]) != team) {
                        out_moves[result++] = attack;
                    }
                    attack = index_advance_right(team, index);
                    if (attack != -1 && game_board[attack] != -1 && CHESS_TEAM(game_board[attack]) != team) {
                        out_moves[result++] = attack;
                    }
                }
            }
            break;
        case CHESS_KNIGHT: {
            signed char i1 = index_advance(team, index);
            signed char i2 = index_advance(team, i1);
            const signed char l2 = index_left(team, i2);
            const signed char r2 = index_right(team, i2);
            signed char tmp = index_left(team, i1);
            const signed char l1 = index_left(team, tmp);
            tmp = index_right(team, i1);
            const signed char r1 = index_right(team, tmp);

            i1 = index_retreat(team, index);
            i2 = index_retreat(team, i1);
            const signed char l4 = index_left(team, i2);
            const signed char r4 = index_right(team, i2);
            tmp = index_left(team, i1);
            const signed char l3 = index_left(team, tmp);
            tmp = index_right(team, i1);
            const signed char r3 = index_right(team, tmp);

            tmp = l1;
            if (tmp != -1 && (game_board[tmp] == -1 || CHESS_TEAM(game_board[tmp]) != team)) {
                out_moves[result++] = tmp;
            }
            tmp = l2;
            if (tmp != -1 && (game_board[tmp] == -1 || CHESS_TEAM(game_board[tmp]) != team)) {
                out_moves[result++] = tmp;
            }
            tmp = r1;
            if (tmp != -1 && (game_board[tmp] == -1 || CHESS_TEAM(game_board[tmp]) != team)) {
                out_moves[result++] = tmp;
            }
            tmp = r2;
            if (tmp != -1 && (game_board[tmp] == -1 || CHESS_TEAM(game_board[tmp]) != team)) {
                out_moves[result++] = tmp;
            }

            tmp = l3;
            if (tmp != -1 && (game_board[tmp] == -1 || CHESS_TEAM(game_board[tmp]) != team)) {
                out_moves[result++] = tmp;
            }
            tmp = l4;
            if (tmp != -1 && (game_board[tmp] == -1 || CHESS_TEAM(game_board[tmp]) != team)) {
                out_moves[result++] = tmp;
            }
            tmp = r3;
            if (tmp != -1 && (game_board[tmp] == -1 || CHESS_TEAM(game_board[tmp]) != team)) {
                out_moves[result++] = tmp;
            }
            tmp = r4;
            if (tmp != -1 && (game_board[tmp] == -1 || CHESS_TEAM(game_board[tmp]) != team)) {
                out_moves[result++] = tmp;
            }
        } break;
        case CHESS_BISHOP: {
            move_until_obstacle(index_advance_left, team, index, game_board, out_moves, &result);
            move_until_obstacle(index_advance_right, team, index, game_board, out_moves, &result);
            move_until_obstacle(index_retreat_left, team, index, game_board, out_moves, &result);
            move_until_obstacle(index_retreat_right, team, index, game_board, out_moves, &result);
        } break;
        case CHESS_ROOK: {
            move_until_obstacle(index_advance, team, index, game_board, out_moves, &result);
            move_until_obstacle(index_left, team, index, game_board, out_moves, &result);
            move_until_obstacle(index_right, team, index, game_board, out_moves, &result);
            move_until_obstacle(index_retreat, team, index, game_board, out_moves, &result);
        } break;
        case CHESS_QUEEN: {
            move_until_obstacle(index_advance, team, index, game_board, out_moves, &result);
            move_until_obstacle(index_left, team, index, game_board, out_moves, &result);
            move_until_obstacle(index_right, team, index, game_board, out_moves, &result);
            move_until_obstacle(index_retreat, team, index, game_board, out_moves, &result);

            move_until_obstacle(index_advance_left, team, index, game_board, out_moves, &result);
            move_until_obstacle(index_advance_right, team, index, game_board, out_moves, &result);
            move_until_obstacle(index_retreat_left, team, index, game_board, out_moves, &result);
            move_until_obstacle(index_retreat_right, team, index, game_board, out_moves, &result);
        } break;
        case CHESS_KING: {
            signed char i = index;
            if (i != -1) {
                i = index_advance(team, i);
                if (i != -1 && (game_board[i] == -1 || CHESS_TEAM(game_board[i]) != team)) {
                    out_moves[result++] = i;
                }
            }
            i = index;
            if (i != -1) {
                i = index_advance_left(team, i);
                if (i != -1 && (game_board[i] == -1 || CHESS_TEAM(game_board[i]) != team)) {
                    out_moves[result++] = i;
                }
            }
            i = index;
            if (i != -1) {
                i = index_advance_right(team, i);
                if (i != -1 && (game_board[i] == -1 || CHESS_TEAM(game_board[i]) != team)) {
                    out_moves[result++] = i;
                }
            }
            i = index;
            if (i != -1) {
                i = index_left(team, i);
                if (i != -1 && (game_board[i] == -1 || CHESS_TEAM(game_board[i]) != team)) {
                    out_moves[result++] = i;
                }
            }
            i = index;
            if (i != -1) {
                i = index_right(team, i);
                if (i != -1 && (game_board[i] == -1 || CHESS_TEAM(game_board[i]) != team)) {
                    out_moves[result++] = i;
                }
            }
            i = index;
            if (i != -1) {
                i = index_retreat(team, i);
                if (i != -1 && (game_board[i] == -1 || CHESS_TEAM(game_board[i]) != team)) {
                    out_moves[result++] = i;
                }
            }
            i = index;
            if (i != -1) {
                i = index_retreat_left(team, i);
                if (i != -1 && (game_board[i] == -1 || CHESS_TEAM(game_board[i]) != team)) {
                    out_moves[result++] = i;
                }
            }
            i = index;
            if (i != -1) {
                i = index_retreat_right(team, i);
                if (i != -1 && (game_board[i] == -1 || CHESS_TEAM(game_board[i]) != team)) {
                    out_moves[result++] = i;
                }
            }
        } break;
    }
    return result;
}
signed char chess_contains_move(const signed char* moves, signed char moves_size, signed char index) {
    for (signed char i = 0; i < moves_size; ++i) {
        if (moves[i] == index) {
            return 1;
        }
    }
    return 0;
}
static signed char is_checked_king(signed char king_index, const signed char* game_board) {
    signed char tmp_moves[64];
    signed char result = 0;
    if (king_index == -1) {
        return 0;
    }
    const signed char id = game_board[king_index];
    if (id == -1) {
        return 0;
    }
    const signed char team = CHESS_TEAM(id);
    signed char c = 0;
    for (signed char i = 0; i < 64; ++i) {
        const signed char id_cmp = game_board[i];
        if (id_cmp != -1) {
            const signed char team_cmp = CHESS_TEAM(id_cmp);
            if (team_cmp != team) {
                // found an opposing piece on the board
                const signed char tmp_moves_size = compute_moves(i, tmp_moves, game_board);
                if (chess_contains_move(tmp_moves, tmp_moves_size, king_index)) {
                    return 1;
                }
            }
        }
    }
    return 0;
}
static signed char compute_check_moves(signed char index, signed char checked_king_index, const signed char* game_board, signed char* out_moves) {
    signed char tmp_moves[64];
    signed char tmp_board[64];
    signed char result = 0;
    const signed char id = game_board[index];
    const signed char checked_king_id = game_board[checked_king_index];
    if (CHESS_KING != CHESS_TYPE(checked_king_id)) {
        return 0;
    }
    if (CHESS_TEAM(checked_king_id) != CHESS_TEAM(id)) {
        return 0;
    }
    signed char tmp_moves_sz = compute_moves(index, tmp_moves, game_board);
    for (signed char i = 0; i < tmp_moves_sz; ++i) {
        memcpy(tmp_board, game_board, sizeof(tmp_board));
        tmp_board[index] = -1;
        tmp_board[tmp_moves[i]] = id;
        if (CHESS_KING == CHESS_TYPE(id)) {
            if (!is_checked_king(tmp_moves[i], tmp_board)) {
                out_moves[result++] = tmp_moves[i];
            }
        } else {
            if (!is_checked_king(checked_king_index, tmp_board)) {
                out_moves[result++] = tmp_moves[i];
            }
        }
    }
    return result;
}
static void eliminate_checked_moves(signed char index, const signed char* kings, const signed char* game_board, signed char* in_out_moves, signed char* in_out_moves_size) {
    if (index == -1) {
        return;
    }
    signed char tmp_board[64];
    const signed char id = game_board[index];
    const signed char team = CHESS_TEAM(id);
    const signed char type = CHESS_TYPE(id);
    if (kings[team] != index) {  // other piece
        const signed char king_index = kings[team];
        for (signed char i = 0; i < *(in_out_moves_size); ++i) {
            memcpy(tmp_board, game_board, sizeof(tmp_board));
            tmp_board[index] = -1;
            tmp_board[in_out_moves[i]] = id;
            if (is_checked_king(king_index, tmp_board)) {
                for (signed char j = i; j < (*in_out_moves_size) - 1; ++j) {
                    in_out_moves[j] = in_out_moves[j + 1];
                }
                --(*in_out_moves_size);
                --i;
            }
        }
    } else {  // king piece
        const signed char king_index = index;
        const signed char king_id = game_board[king_index];
        for (signed char i = 0; i < (*in_out_moves_size); ++i) {
            memcpy(tmp_board, game_board, sizeof(tmp_board));
            tmp_board[king_index] = -1;
            const signed char move = in_out_moves[i];
            tmp_board[move] = king_id;
            if (is_checked_king(move, tmp_board)) {
                for (signed char j = i; j < (*in_out_moves_size) - 1; ++j) {
                    in_out_moves[j] = in_out_moves[j + 1];
                }
                --(*in_out_moves_size);
                --i;
            }
        }
    }
}
void chess_init(chess_game_t* out_game) {
    out_game->turn = 0;
    for (signed char i = 0; i < 64; ++i) {
        out_game->board[i] = -1;
    }
    for (signed char i = 0; i < 8; ++i) {
        out_game->board[8 + i] = CHESS_ID(0, CHESS_PAWN);
        out_game->board[48 + i] = CHESS_ID(1, CHESS_PAWN);
    }
    out_game->board[0] = CHESS_ID(0, CHESS_ROOK);
    out_game->board[7] = CHESS_ID(0, CHESS_ROOK);
    out_game->board[1] = CHESS_ID(0, CHESS_KNIGHT);
    out_game->board[6] = CHESS_ID(0, CHESS_KNIGHT);
    out_game->board[2] = CHESS_ID(0, CHESS_BISHOP);
    out_game->board[5] = CHESS_ID(0, CHESS_BISHOP);
    out_game->board[3] = CHESS_ID(0, CHESS_KING);
    out_game->kings[0] = 3;
    out_game->board[4] = CHESS_ID(0, CHESS_QUEEN);

    out_game->board[56 + 0] = CHESS_ID(1, CHESS_ROOK);
    out_game->board[56 + 7] = CHESS_ID(1, CHESS_ROOK);
    out_game->board[56 + 1] = CHESS_ID(1, CHESS_KNIGHT);
    out_game->board[56 + 6] = CHESS_ID(1, CHESS_KNIGHT);
    out_game->board[56 + 2] = CHESS_ID(1, CHESS_BISHOP);
    out_game->board[56 + 5] = CHESS_ID(1, CHESS_BISHOP);
    out_game->board[56 + 3] = CHESS_ID(1, CHESS_QUEEN);
    out_game->board[56 + 4] = CHESS_ID(1, CHESS_KING);
    out_game->kings[1] = 56 + 4;
}
signed char chess_move(chess_game_t* game, signed char index_from, signed char index_to) {
    if (game == NULL || index_from < 0 || index_from > 63 || index_to < 0 || index_to > 63 || index_from==index_to) {
        return 0;
    }

    const signed char id = game->board[index_from];
    if (game->turn != CHESS_TEAM(id)) {
        return 0;
    }
    signed char tmp_moves[64];
    signed char tmp_moves_size = 0;
    const signed char king_index = game->kings[CHESS_TEAM(id)];
    if(is_checked_king(king_index,game->board)) {
        tmp_moves_size = compute_check_moves(index_from,king_index, game->board, tmp_moves);
    } else {
        tmp_moves_size = compute_moves(index_from, tmp_moves, game->board);
    }
    if(chess_contains_move(tmp_moves, tmp_moves_size, index_to)) {
        game->board[index_to] = game->board[index_from];
        game->board[index_from] = -1;
        if(++game->turn>1) { game->turn = 0; }
        if(CHESS_TYPE(id)==CHESS_KING) {
            game->kings[CHESS_TEAM(id)]=index_to;
        }
        return 1;    
    }
    return 0;
}
signed char chess_compute_moves(const chess_game_t* game, signed char index, signed char* out_moves) {
    if (game == NULL || index < 0 || index > 63) {
        return 0;
    }
    const signed char id = game->board[index];
    if (game->turn != CHESS_TEAM(id)) {
        return 0;
    }
    signed char result = 0;
    const signed char king_index = game->kings[CHESS_TEAM(id)];
    if(is_checked_king(king_index,game->board)) {
        result = compute_check_moves(index,king_index, game->board, out_moves);
    } else {
        result = compute_moves(index, out_moves, game->board);
        eliminate_checked_moves(index,game->kings,game->board,out_moves,&result);
    }
    return result;
}
signed char chess_turn(const chess_game_t* game) {
    return game->turn;
}
signed char chess_index_to_id(const chess_game_t* game, signed char index) {
    if(game==NULL || index<0 || index>63) {
        return -1;
    }
    return game->board[index];
}
chess_status_t chess_status(const chess_game_t* game, signed char team) {
    const signed char index = game->kings[team];
    signed char moves[64];
    if(is_checked_king(index,game->board)) {
        if(0==chess_compute_moves(game,index,moves)) {
            return CHESS_MATE;
        }
        return CHESS_CHECK;
    }
    return CHESS_NORMAL;
}