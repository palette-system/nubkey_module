/* Copyright 2021 takashicompany
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include QMK_KEYBOARD_H

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [0] = LAYOUT_ortho_1x1(
        KC_MS_BTN1
    )
};

void pointing_device_init_kb(void) {
    // _delay_ms(100); // nubkey_module の起動よりもProMicroの起動のが早い場合用 nubkey_module の起動を待ってから設定コマンドを投げる
    uint8_t res[8];
    uint16_t timeout=100;
    i2c_status_t status;
    uint8_t addr=0x0A << 1;
    // 送信するコマンド
    uint8_t cmd[]={0x48,   /* 左 */ 0x03, 0x20,   /* 右 */ 0x03, 0x20,   /* 上 */ 0x02, 0x58,   /* 下 */ 0x02, 0x58}; // マウス移動の速度 左右上下
    // uint8_t cmd[]={0x45, 0x01, 0x68}; // 高さ調節
    // uint8_t cmd[]={0x42, 0x00}; // nubkey off モード
    // uint8_t cmd[]={0x4A}; // 設定リセット
    // コマンドの送信
    status  = i2c_transmit(addr, cmd, sizeof(cmd), timeout);
    if (status != 0) return;
    status = i2c_receive(addr, res, 1, timeout);
}


