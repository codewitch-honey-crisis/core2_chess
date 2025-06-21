#define SPI_PORT SPI3_HOST
#define SPI_CLK 18
#define SPI_MISO 38
#define SPI_MOSI 23

#define SD_PORT SPI3_HOST
#define SD_CS 4
// UIX can draw to one buffer while sending
// another for better performance but it requires
// twice the transfer buffer memory
#define LCD_TWO_BUFFERS  // optional
// screen dimensions
#define LCD_WIDTH 320
#define LCD_HEIGHT 240
// indicates how much of the screen gets updated at once
// #define LCD_DIVISOR 2 // optional
// screen connections
#define LCD_PORT SPI3_HOST
#define LCD_DC 15
#define LCD_CS 5
#define LCD_RST -1    // optional
#define LCD_BL -1     // optional
#define LCD_BL_LOW 0  // optional
#define LCD_PANEL esp_lcd_new_panel_ili9342
#define LCD_GAP_X 0                   // optional
#define LCD_GAP_Y 0                   // optional
#define LCD_SWAP_XY 0                 // optional
#define LCD_MIRROR_X 0                // optional
#define LCD_MIRROR_Y 0                // optional
#define LCD_INVERT_COLOR 1            // optional
#define LCD_BGR 1                     // optional
#define LCD_BIT_DEPTH 16              // optional
#define LCD_SPEED (40 * 1000 * 1000)  // optional

#if __has_include(<Arduino.h>)
#include <Arduino.h>
#endif

#include <esp_i2c.hpp>  // i2c initialization
#include <ft6336.hpp>
#include <gfx.hpp>            // graphics library
#include <m5core2_power.hpp>  // AXP192 power management (core2)
#include <uix.hpp>            // user interface library

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/uart.h"
#include "esp_lcd_panel_ili9342.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_spiffs.h"
#include "esp_vfs_fat.h"
#define CB24_IMPLEMENTATION
#include "assets/cb24.hpp"

// namespace imports
#ifdef ARDUINO
using namespace arduino;  // devices
#else
using namespace esp_idf;  // devices
#endif
using namespace gfx;  // graphics
using namespace uix;  // user interface

using color_t = color<rgb_pixel<16>>;     // screen color
using color32_t = color<rgba_pixel<32>>;  // UIX color

using screen_t = uix::screen<rgb_pixel<LCD_BIT_DEPTH>>;
using surface_t = screen_t::control_surface_type;

static uix::display lcd;

static void power_init() {
    // for AXP192 power management
    static m5core2_power power(esp_i2c<1, 21, 22>::instance);
    // draw a little less power
    power.initialize();
    power.lcd_voltage(3.0);
}

static void spi_init() {
    spi_bus_config_t buscfg;
    memset(&buscfg, 0, sizeof(buscfg));
    buscfg.sclk_io_num = SPI_CLK;
    buscfg.mosi_io_num = SPI_MOSI;
    buscfg.miso_io_num = SPI_MISO;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
#ifdef LCD_DIVISOR
    static constexpr const size_t lcd_divisor = LCD_DIVISOR;
#else
    static constexpr const size_t lcd_divisor = 10;
#endif
#ifdef LCD_BIT_DEPTH
    static constexpr const size_t lcd_pixel_size = (LCD_BIT_DEPTH + 7) / 8;
#else
    static constexpr const size_t lcd_pixel_size = 2;
#endif
    // the size of our transfer buffer(s)
    static const constexpr size_t lcd_transfer_buffer_size =
        LCD_WIDTH * LCD_HEIGHT * lcd_pixel_size / lcd_divisor;

    buscfg.max_transfer_sz =
        (lcd_transfer_buffer_size > 512 ? lcd_transfer_buffer_size : 512) + 8;
    // Initialize the SPI bus on VSPI (SPI3)
    spi_bus_initialize(SPI_PORT, &buscfg, SPI_DMA_CH_AUTO);
}

// initialize the screen using the esp panel API
static void lcd_init() {
    // for the touch panel
    using touch_t = ft6336<320, 280, 16>;
    static touch_t touch(esp_i2c<1, 21, 22>::instance);

#ifdef LCD_DIVISOR
    static constexpr const size_t lcd_divisor = LCD_DIVISOR;
#else
    static constexpr const size_t lcd_divisor = 10;
#endif
#ifdef LCD_BIT_DEPTH
    static constexpr const size_t lcd_pixel_size = (LCD_BIT_DEPTH + 7) / 8;
#else
    static constexpr const size_t lcd_pixel_size = 2;
#endif
    // the size of our transfer buffer(s)
    static const constexpr size_t lcd_transfer_buffer_size =
        LCD_WIDTH * LCD_HEIGHT * lcd_pixel_size / lcd_divisor;

    uint8_t* lcd_transfer_buffer1 =
        (uint8_t*)heap_caps_malloc(lcd_transfer_buffer_size, MALLOC_CAP_DMA);
    uint8_t* lcd_transfer_buffer2 =
        (uint8_t*)heap_caps_malloc(lcd_transfer_buffer_size, MALLOC_CAP_DMA);
    if (lcd_transfer_buffer1 == nullptr || lcd_transfer_buffer2 == nullptr) {
        puts("Out of memory allocating transfer buffers");
        while (1) vTaskDelay(5);
    }
#if defined(LCD_BL) && LCD_BL > 1
#ifdef LCD_BL_LOW
    static constexpr const int bl_on = !(LCD_BL_LOW);
#else
    static constexpr const int bl_on = 1;
#endif
    gpio_set_direction((gpio_num_t)LCD_BL, GPIO_MODE_OUTPUT);
    gpio_set_level((gpio_num_t)LCD_BL, !bl_on);
#endif
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config;
    memset(&io_config, 0, sizeof(io_config));
    io_config.dc_gpio_num = LCD_DC;
    io_config.cs_gpio_num = LCD_CS;
#ifdef LCD_SPEED
    io_config.pclk_hz = LCD_SPEED;
#else
    io_config.pclk_hz = 20 * 1000 * 1000;
#endif
    io_config.lcd_cmd_bits = 8;
    io_config.lcd_param_bits = 8;
    io_config.spi_mode = 0;
    io_config.trans_queue_depth = 10;
    io_config.on_color_trans_done = [](esp_lcd_panel_io_handle_t lcd_io,
                                       esp_lcd_panel_io_event_data_t* edata,
                                       void* user_ctx) {
        lcd.flush_complete();
        return true;
    };
    // Attach the LCD to the SPI bus
    esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_PORT, &io_config,
                             &io_handle);

    esp_lcd_panel_handle_t lcd_handle = NULL;
    esp_lcd_panel_dev_config_t lcd_config;
    memset(&lcd_config, 0, sizeof(lcd_config));
#ifdef LCD_RST
    lcd_config.reset_gpio_num = LCD_RST;
#else
    lcd_config.reset_gpio_num = -1;
#endif
#if defined(LCD_BGR) && LCD_BGR != 0
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    lcd_config.rgb_endian = LCD_RGB_ENDIAN_BGR;
#else
    lcd_config.color_space = ESP_LCD_COLOR_SPACE_BGR;
#endif
#else
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    lcd_config.rgb_endian = LCD_RGB_ENDIAN_RGB;
#else
    lcd_config.color_space = ESP_LCD_COLOR_SPACE_RGB;
#endif
#endif
#ifdef LCD_BIT_DEPTH
    lcd_config.bits_per_pixel = LCD_BIT_DEPTH;
#else
    lcd_config.bits_per_pixel = 16;
#endif

    // Initialize the LCD configuration
    LCD_PANEL(io_handle, &lcd_config, &lcd_handle);

    // Reset the display
    esp_lcd_panel_reset(lcd_handle);

    // Initialize LCD panel
    esp_lcd_panel_init(lcd_handle);
#ifdef LCD_GAP_X
    static constexpr int lcd_gap_x = LCD_GAP_X;
#else
    static constexpr int lcd_gap_x = 0;
#endif
#ifdef LCD_GAP_Y
    static constexpr int lcd_gap_y = LCD_GAP_Y;
#else
    static constexpr int lcd_gap_y = 0;
#endif
    esp_lcd_panel_set_gap(lcd_handle, lcd_gap_x, lcd_gap_y);
#ifdef LCD_SWAP_XY
    esp_lcd_panel_swap_xy(lcd_handle, LCD_SWAP_XY);
#endif
#ifdef LCD_MIRROR_X
    static constexpr int lcd_mirror_x = LCD_MIRROR_X;
#else
    static constexpr int lcd_mirror_x = 0;
#endif
#ifdef LCD_MIRROR_Y
    static constexpr int lcd_mirror_y = LCD_MIRROR_Y;
#else
    static constexpr int lcd_mirror_y = 0;
#endif
    esp_lcd_panel_mirror(lcd_handle, lcd_mirror_x, lcd_mirror_y);
#ifdef LCD_INVERT_COLOR
    esp_lcd_panel_invert_color(lcd_handle, LCD_INVERT_COLOR);
#endif

    // Turn on the screen
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    esp_lcd_panel_disp_on_off(lcd_handle, true);
#else
    esp_lcd_panel_disp_off(lcd_handle, false);
#endif
#if defined(LCD_BL) && LCD_BL > 1
    gpio_set_level((gpio_num_t)LCD_BL, bl_on);
#endif
    lcd.buffer_size(lcd_transfer_buffer_size);
    lcd.buffer1(lcd_transfer_buffer1);
    lcd.buffer2(lcd_transfer_buffer2);
    lcd.on_flush_callback(
        [](const rect16& bounds, const void* bmp, void* state) {
            int x1 = bounds.x1, y1 = bounds.y1, x2 = bounds.x2 + 1,
                y2 = bounds.y2 + 1;
            esp_lcd_panel_draw_bitmap((esp_lcd_panel_handle_t)state, x1, y1, x2,
                                      y2, (void*)bmp);
        },
        lcd_handle);
    lcd.on_touch_callback(
        [](point16* out_locations, size_t* in_out_locations_size, void* state) {
            touch.update();
            // UIX supports multiple touch points.
            // so does the FT6336 so we potentially have
            // two values
            *in_out_locations_size = 0;
            uint16_t x, y;
            if (touch.xy(&x, &y)) {
                out_locations[0] = point16(x, y);
                ++*in_out_locations_size;
                if (touch.xy2(&x, &y)) {
                    out_locations[1] = point16(x, y);
                    ++*in_out_locations_size;
                }
            }
        });
    touch.initialize();
    touch.rotation(0);
}

static sdmmc_card_t* sd_card = nullptr;
static bool sd_init() {
    static const char mount_point[] = "/sdcard";
    esp_vfs_fat_sdmmc_mount_config_t mount_config;
    memset(&mount_config, 0, sizeof(mount_config));
    mount_config.format_if_mount_failed = false;
    mount_config.max_files = 5;
    mount_config.allocation_unit_size = 0;

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SD_PORT;
    // // This initializes the slot without card detect (CD) and write protect (WP)
    // // signals.
    sdspi_device_config_t slot_config;
    memset(&slot_config, 0, sizeof(slot_config));
    slot_config.host_id = (spi_host_device_t)SD_PORT;
    slot_config.gpio_cs = (gpio_num_t)SD_CS;
    slot_config.gpio_cd = SDSPI_SLOT_NO_CD;
    slot_config.gpio_wp = SDSPI_SLOT_NO_WP;
    slot_config.gpio_int = GPIO_NUM_NC;
    if (ESP_OK != esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config,
                                          &mount_config, &sd_card)) {
        return false;
    }
    return true;
}
static void spiffs_init() {
    esp_vfs_spiffs_conf_t conf;
    memset(&conf, 0, sizeof(conf));
    conf.base_path = "/spiffs";
    conf.partition_label = NULL;
    conf.max_files = 5;
    conf.format_if_mount_failed = true;
    if (ESP_OK != esp_vfs_spiffs_register(&conf)) {
        puts("Unable to initialize SPIFFS");
        while (1) vTaskDelay(5);
    }
}

static screen_t main_screen;
typedef enum {
    CHESS_PAWN = 0,
    CHESS_BISHOP = 1,
    CHESS_ROOK = 2,
    CHESS_KNIGHT = 3,
    CHESS_QUEEN = 4,
    CHESS_KING = 5
} chess_piece_type_t;
#define CHESS_TEAM(x) (!!(x & (1 << 3)))
#define CHESS_TYPE(x) ((chess_piece_type_t)(x & 7))
#define CHESS_ID(x, y) ((x ? (1 << 3) : (0 << 3)) | (int)y)
#define CHESS_NONE (-1)
typedef struct {
    const int id;
    const const_bitmap<alpha_pixel<CB24_BIT_DEPTH>>& icon;
} chess_piece_t;

static const chess_piece_t chess_pieces[] = {
    {CHESS_ID(0, CHESS_PAWN), cb24_chess_pawn},
    {CHESS_ID(0, CHESS_BISHOP), cb24_chess_bishop},
    {CHESS_ID(0, CHESS_ROOK), cb24_chess_rook},
    {CHESS_ID(0, CHESS_KNIGHT), cb24_chess_knight},
    {CHESS_ID(0, CHESS_QUEEN), cb24_chess_queen},
    {CHESS_ID(0, CHESS_KING), cb24_chess_king},
    {CHESS_ID(1, CHESS_PAWN), cb24_chess_pawn},
    {CHESS_ID(1, CHESS_BISHOP), cb24_chess_bishop},
    {CHESS_ID(1, CHESS_ROOK), cb24_chess_rook},
    {CHESS_ID(1, CHESS_KNIGHT), cb24_chess_knight},
    {CHESS_ID(1, CHESS_QUEEN), cb24_chess_queen},
    {CHESS_ID(1, CHESS_KING), cb24_chess_king}};
#define CHESS_ID_TO_PIECE(x) (chess_pieces[CHESS_TEAM(x) * 6 + CHESS_TYPE(x)])


template <typename ControlSurfaceType>
class chess_board : public control<ControlSurfaceType> {
    using base_type = control<ControlSurfaceType>;
    int board[64];
    int moves[64];
    int moves_size;
    int check_moves[64];
    int check_moves_size;
    int touched;
    spoint16 last_touch;
    int turn;
    int check;
    int kings[2];
    int move_count;
    void init_board() {
        turn = 0;
        check = -1;
        moves_size = 0;
        move_count = 0;
        for (int i = 0; i < 64; ++i) {
            board[i] = -1;
        }
        for (int i = 0; i < 8; ++i) {
            board[8 + i] = CHESS_ID(0, CHESS_PAWN);
            board[48 + i] = CHESS_ID(1, CHESS_PAWN);
        }
        board[0] = CHESS_ID(0, CHESS_ROOK);
        board[7] = CHESS_ID(0, CHESS_ROOK);
        board[1] = CHESS_ID(0, CHESS_KNIGHT);
        board[6] = CHESS_ID(0, CHESS_KNIGHT);
        board[2] = CHESS_ID(0, CHESS_BISHOP);
        board[5] = CHESS_ID(0, CHESS_BISHOP);
        board[3] = CHESS_ID(0, CHESS_QUEEN);
        board[4] = CHESS_ID(0, CHESS_KING);
        kings[0] = 4;
        board[56 + 0] = CHESS_ID(1, CHESS_ROOK);
        board[56 + 7] = CHESS_ID(1, CHESS_ROOK);
        board[56 + 1] = CHESS_ID(1, CHESS_KNIGHT);
        board[56 + 6] = CHESS_ID(1, CHESS_KNIGHT);
        board[56 + 2] = CHESS_ID(1, CHESS_BISHOP);
        board[56 + 5] = CHESS_ID(1, CHESS_BISHOP);
        board[56 + 3] = CHESS_ID(1, CHESS_KING);
        kings[1] = 56 + 3;
        board[56 + 4] = CHESS_ID(1, CHESS_QUEEN);
        touched = -1;
    }
    int id_to_index(int id) {
        for (int i = 0; i < 64; ++i) {
            if (id == board[i]) {
                return i;
            }
        }
        return -1;
    }
    void compute_checked_team() {
        int tmp_moves[64];
        for (int i = 0; i < 64; ++i) {
            if (board[i] != -1) {
                const int board_id = board[i];
                const int board_team = CHESS_TEAM(board_id);
                const int sz = compute_moves(i, tmp_moves, board);
                for (int j = 0; j < sz; ++j) {
                    int move_id = board[tmp_moves[j]];
                    if (CHESS_TEAM(move_id) != board_team && CHESS_TYPE(move_id) == CHESS_KING) {
                        check = CHESS_TEAM(move_id);
                        return;
                    }
                }
            }
        }
        check = -1;
    }
    bool is_checked_king(int king_index, const int* game_board) {
        int tmp_moves[64];
        int result = 0;
        if (king_index == -1) {
            return false;
        }
        const int id = game_board[king_index];
        if (id == -1) {
            return false;
        }
        const int team = CHESS_TEAM(id);
        int c = 0;
        for (int i = 0; i < 64; ++i) {
            const int id_cmp = game_board[i];
            if (id_cmp != -1) {
                const int team_cmp = CHESS_TEAM(id_cmp);
                if (team_cmp != team) {
                    // found an opposing piece on the board
                    const int tmp_moves_size = compute_moves(i, tmp_moves, game_board);
                    if (contains_move(tmp_moves, tmp_moves_size, king_index)) {
                        return true;
                    }
                }
            }
        }
        return false;
    }
    int compute_check_moves(int index, int checked_king_index, const int* game_board, int* out_moves) {
        int tmp_moves[64];
        int tmp_board[64];
        int result = 0;
        const int id = game_board[index];
        const int checked_king_id = game_board[checked_king_index];
        if (CHESS_KING != CHESS_TYPE(checked_king_id)) {
            return 0;
        }
        if (CHESS_TEAM(checked_king_id) != CHESS_TEAM(id)) {
            return 0;
        }
        int tmp_moves_sz = compute_moves(index, tmp_moves, game_board);
        for (int i = 0; i < tmp_moves_sz; ++i) {
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
    void eliminate_checked_moves(int index, const int* game_board, int* in_out_moves, int* in_out_moves_size) {
        if (index == -1) {
            return;
        }
        int tmp_board[64];
        const int id = game_board[index];
        const int team = CHESS_TEAM(id);
        const int type = CHESS_TYPE(id);
        if (kings[team] != index) {  // other piece
            const int king_index = kings[team];
            for (int i = 0; i < *(in_out_moves_size); ++i) {
                memcpy(tmp_board, game_board, sizeof(tmp_board));
                tmp_board[index] = -1;
                tmp_board[in_out_moves[i]] = id;
                if (is_checked_king(king_index, tmp_board)) {
                    for (int j = i; j < (*in_out_moves_size) - 1; ++j) {
                        in_out_moves[j] = in_out_moves[j + 1];
                    }
                    --(*in_out_moves_size);
                    --i;
                }
            }
        } else {  // king piece
            const int king_index = index;
            const int king_id = game_board[king_index];
            for (int i = 0; i < (*in_out_moves_size); ++i) {
                memcpy(tmp_board, game_board, sizeof(tmp_board));
                tmp_board[king_index] = -1;
                const int move = in_out_moves[i];
                tmp_board[move] = king_id;
                if (is_checked_king(move, tmp_board)) {
                    for (int j = i; j < (*in_out_moves_size) - 1; ++j) {
                        in_out_moves[j] = in_out_moves[j + 1];
                    }
                    --(*in_out_moves_size);
                    --i;
                }
            }
        }
    }
    int compute_moves(int index, int* out_moves, const int* game_board) {
        const int id = game_board[index];
        if (id == -1) {
            return 0;
        }
        const int type = CHESS_TYPE(id);
        const int team = CHESS_TEAM(id);
        int result = 0;
        switch (type) {
            case CHESS_PAWN:
                if ((team == 0 && index < 16) || (team == 1 && index > (64 - 16))) {  // the pawn is on its first move
                    int tmp = index_advance(team, index);
                    if (tmp != -1) {
                        out_moves[result++] = tmp;
                        int attack = index_left(team, tmp);
                        if (attack != -1 && game_board[attack] != -1 && CHESS_TEAM(game_board[attack]) != team) {
                            out_moves[result++] = attack;
                        }
                        attack = index_right(team, tmp);
                        if (attack != -1 && game_board[attack] != -1 && CHESS_TEAM(game_board[attack]) != team) {
                            out_moves[result++] = attack;
                        }
                    }
                    tmp = index_advance(team, tmp);
                    if (tmp != -1) {
                        out_moves[result++] = tmp;
                    }
                } else {
                    int tmp = index_advance(team, index);
                    if (tmp != -1) {
                        out_moves[result++] = tmp;
                        int attack = index_left(team, tmp);
                        if (attack != -1 && game_board[attack] != -1 && CHESS_TEAM(game_board[attack]) != team) {
                            out_moves[result++] = attack;
                        }
                        attack = index_right(team, tmp);
                        if (attack != -1 && game_board[attack] != -1 && CHESS_TEAM(game_board[attack]) != team) {
                            out_moves[result++] = attack;
                        }
                    }
                }
                break;
            case CHESS_KNIGHT: {
                int i1 = index_advance(team, index);
                int i2 = index_advance(team, i1);
                const int l2 = index_left(team, i2);
                const int r2 = index_right(team, i2);
                int tmp = index_left(team, i1);
                const int l1 = index_left(team, tmp);
                tmp = index_right(team, i1);
                const int r1 = index_right(team, tmp);

                i1 = index_retreat(team, index);
                i2 = index_retreat(team, i1);
                const int l4 = index_left(team, i2);
                const int r4 = index_right(team, i2);
                tmp = index_left(team, i1);
                const int l3 = index_left(team, tmp);
                tmp = index_right(team, i1);
                const int r3 = index_right(team, tmp);

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
                int i = index;
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
    int point_to_square(spoint16 point) {
        const int16_t extent = this->dimensions().aspect_ratio() >= 1 ? this->dimensions().height : this->dimensions().width;
        const int x = point.x / (extent / 8);
        const int y = point.y / (extent / 8);
        return y * 8 + x;
    }
    void square_coords(int index, srect16* out_rect) {
        const int16_t extent = this->dimensions().aspect_ratio() >= 1 ? this->dimensions().height : this->dimensions().width;
        const ssize16 square_size(extent / 8, extent / 8);
        const int x = index % 8;
        const int y = index / 8;
        const spoint16 origin(x * (extent / 8), y * (extent / 8));
        *out_rect = srect16(origin, square_size);
    }
    static bool contains_move(int* moves, int moves_size, int index) {
        for (int i = 0; i < moves_size; ++i) {
            if (moves[i] == index) {
                return true;
            }
        }
        return false;
    }
    static void move_until_obstacle(int (*index_fn)(int team, int index), int team, int index, const int* game_board, int* out_moves, int* out_size) {
        int i = index;
        while (true) {
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
    static int index_advance(int team, int index) {
        if (index == -1) return -1;
        if (team == 0) {
            index += 8;
        } else if (team == 1) {
            index -= 8;
        }
        if (index < 0 || index > 63) return -1;
        return index;
    }
    static int index_advance_left(int team, int index) {
        if (index == -1) return -1;
        const int x = index % 8;
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
    static int index_advance_right(int team, int index) {
        if (index == -1) return -1;
        const int x = index % 8;
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
    static int index_retreat_left(int team, int index) {
        if (index == -1) return -1;
        const int x = index % 8;
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
    static int index_retreat_right(int team, int index) {
        if (index == -1) return -1;
        const int x = index % 8;
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
    static int index_retreat(int team, int index) {
        if (index == -1) return -1;
        if (team == 1) {
            index += 8;
        } else if (team == 0) {
            index -= 8;
        }
        if (index < 0 || index > 63) return -1;
        return index;
    }
    static int index_left(int team, int index) {
        if (index == -1) return -1;
        const int x = index % 8;
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
    static int index_right(int team, int index) {
        if (index == -1) return -1;
        const int x = index % 8;
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
    static const const_bitmap<alpha_pixel<4>>& chess_icon(int id) {
        const int type = CHESS_TYPE(id);
        switch (type) {
            case CHESS_PAWN:
                return cb24_chess_pawn;
            case CHESS_KNIGHT:
                return cb24_chess_knight;
            case CHESS_BISHOP:
                return cb24_chess_bishop;
            case CHESS_ROOK:
                return cb24_chess_rook;
            case CHESS_QUEEN:
                return cb24_chess_queen;
            case CHESS_KING:
                return cb24_chess_king;
        }
        assert(false);  // invalid piece
        return cb24_chess_pawn;
    }

   public:
    using control_surface_type = ControlSurfaceType;
    using pixel_type = typename ControlSurfaceType::pixel_type;
    using palette_type = typename ControlSurfaceType::palette_type;
    /// @brief Moves a chess_board control
    /// @param rhs The control to move
    chess_board(chess_board&& rhs) {
        do_move_control(rhs);
    }
    /// @brief Moves a chess_board control
    /// @param rhs The control to move
    /// @return this
    chess_board& operator=(chess_board&& rhs) {
        do_move_control(rhs);
        return *this;
    }
    /// @brief Copies a chess_board control
    /// @param rhs The control to copy
    chess_board(const chess_board& rhs) {
        do_copy_control(rhs);
    }
    /// @brief Copies a chess_board control
    /// @param rhs The control to copy
    /// @return this
    chess_board& operator=(const chess_board& rhs) {
        do_copy_control(rhs);
        return *this;
    }
    /// @brief Constructs a chess_board from a given parent with an optional palette
    /// @param parent The parent the control is bound to - usually the screen
    /// @param palette The palette associated with the control. This is usually the screen's palette.
    chess_board(invalidation_tracker& parent, const palette_type* palette = nullptr) : base_type(parent, palette) {
        init_board();
    }
    /// @brief Constructs a chess_board from a given parent with an optional palette
    chess_board() : base_type() {
        init_board();
    }

   protected:
    void do_move_control(chess_board& rhs) {
        do_copy_control(rhs);
    }
    void do_copy_control(chess_board& rhs) {
        memcpy(board, rhs.board, 64 * sizeof(int));
        memcpy(board, rhs.board, 64 * sizeof(int));
        if (rhs.moves_size) {
            memcpy(moves, rhs.moves, rhs.moves_size * sizeof(int));
        }
        moves_size = rhs.moves_size;
        turn = rhs.turn;
        move_count = rhs.move_count;
        last_touch = rhs.last_touch;
    }
    void on_paint(control_surface_type& destination, const srect16& clip) override {
        const int16_t extent = destination.dimensions().aspect_ratio() >= 1 ? destination.dimensions().height : destination.dimensions().width;
        const ssize16 square_size(extent / 8, extent / 8);
        bool toggle = false;
        int idx = 0;
        for (int y = 0; y < extent; y += square_size.height) {
            int i = toggle;
            for (int x = 0; x < extent; x += square_size.width) {
                const srect16 square(spoint16(x, y), square_size);
                if (square.intersects(clip)) {
                    const int id = board[idx];
                    pixel_type px_bg = (i & 1) ? color_t::brown : color_t::tan;
                    pixel_type px_bd = (i & 1) ? color_t::gold : color_t::black;
                    if (id > -1 && check == CHESS_TEAM(id) && CHESS_TYPE(id) == CHESS_KING) {
                        px_bd = color_t::red;
                    }
                    if (touched == idx || contains_move(moves, moves_size, idx)) {
                        px_bg = color_t::light_blue;
                        px_bd = color_t::cornflower_blue;
                    }
                    draw::filled_rectangle(destination, square, px_bg);
                    draw::rectangle(destination, square.inflate(-2, -2), px_bd);
                    if (CHESS_NONE != id) {
                        auto ico = chess_icon(id);
                        const srect16 bounds = ((srect16)ico.bounds()).center(square_size.bounds()).offset(x, y);
                        pixel_type px_piece = CHESS_TEAM(id) ? color_t::white : color_t::black;
                        draw::icon(destination, bounds.location(), ico, px_piece);
                    }
                }
                ++i;
                ++idx;
            }
            toggle = !toggle;
        }
    }
    bool on_touch(size_t locations_size, const spoint16* locations) {
        if (touched > -1) {
            if (locations_size) last_touch = locations[0];
            return true;
        }
        if (locations_size) {
            const int16_t extent = this->dimensions().aspect_ratio() >= 1 ? this->dimensions().height : this->dimensions().width;
            const srect16 square(spoint16::zero(), ssize16(extent / 8, extent / 8));
            int sq = point_to_square(*locations);
            if (sq > -1) {
                const int id = board[sq];
                if (id > -1) {
                    const int team = CHESS_TEAM(id);
                    if (turn == team) {
                        touched = sq;
                        if (is_checked_king(kings[team], board)) {
                            moves_size = compute_check_moves(sq, kings[team], board, moves);
                        } else {
                            moves_size = compute_moves(touched, moves, board);
                            eliminate_checked_moves(touched, board, moves, &moves_size);
                        }
                        srect16 sq_bnds;
                        square_coords(sq, &sq_bnds);
                        this->invalidate(sq_bnds);
                    }
                }
                if (moves_size > 0) {
                    for (size_t i = 0; i < moves_size; ++i) {
                        srect16 sq_bnds;
                        square_coords(moves[i], &sq_bnds);
                        this->invalidate(sq_bnds);
                    }
                    return true;
                }
            }
        }
        return false;
    }
    void on_release() override {
        if (touched > -1) {
            const int id = board[touched];
            const bool is_king = (CHESS_TYPE(id) == CHESS_KING);
            const int team = CHESS_TEAM(id);
            const int16_t extent = this->dimensions().aspect_ratio() >= 1 ? this->dimensions().height : this->dimensions().width;
            const srect16 square(spoint16::zero(), ssize16(extent / 8, extent / 8));
            const int x = touched % 8 * (extent / 8), y = touched / 8 * (extent / 8);
            this->invalidate(square.offset(x, y));
            if (moves_size > 0) {
                for (size_t i = 0; i < moves_size; ++i) {
                    srect16 sq_bnds;
                    square_coords(moves[i], &sq_bnds);
                    this->invalidate(sq_bnds);
                }
                const int release_idx = point_to_square(last_touch);
                if (release_idx != -1) {
                    if (release_idx != touched) {
                        if (is_king) {
                            kings[team] = release_idx;
                        }
                        for (int i = 0; i < moves_size; ++i) {
                            if (release_idx == moves[i]) {
                                board[release_idx] = board[touched];
                                board[touched] = -1;
                                ++turn;
                                if (turn > 1) {
                                    turn = 0;
                                }
                                ++move_count;
                                break;
                            }
                        }
                    }
                }
            }
            moves_size = 0;
        }
        touched = -1;
    }
};

using chess_board_t = chess_board<surface_t>;

chess_board_t board;

#ifdef ARDUINO
void setup() {
    // Serial.begin(115200);
    printf("Arduino version: %d.%d.%d\n", ESP_ARDUINO_VERSION_MAJOR,
           ESP_ARDUINO_VERSION_MINOR, ESP_ARDUINO_VERSION_PATCH);
    printf("ESP-IDF version: %d.%d.%d\n", ESP_IDF_VERSION_MAJOR,
           ESP_IDF_VERSION_MINOR, ESP_IDF_VERSION_PATCH);
#else
void loop();
static void loop_task(void* arg) {
    uint32_t ts = pdTICKS_TO_MS(xTaskGetTickCount());
    while (1) {
        loop();
        uint32_t ms = pdTICKS_TO_MS(xTaskGetTickCount());
        if (ms > ts + 200) {
            ms = pdTICKS_TO_MS(xTaskGetTickCount());
            vTaskDelay(5);
        }
    }
}
extern "C" void app_main() {
    printf("ESP-IDF version: %d.%d.%d\n", ESP_IDF_VERSION_MAJOR,
           ESP_IDF_VERSION_MINOR, ESP_IDF_VERSION_PATCH);
#endif
    power_init();  // do this first
    spi_init();    // used by the LCD and SD reader
    // initialize the display
    lcd_init();
    spiffs_init();
    main_screen.dimensions({LCD_WIDTH, LCD_HEIGHT});
    main_screen.background_color(color_t::black);
    board.bounds(srect16(0, 0, 239, 239).center(main_screen.bounds()));
    main_screen.register_control(board);
    // set the display to our main screen
    lcd.active_screen(main_screen);
#ifndef ARDUINO
    TaskHandle_t loop_handle;
    xTaskCreate(loop_task, "loop_task", 4096, nullptr, 10, &loop_handle);
#endif
}
void loop() {
    lcd.update();
}
