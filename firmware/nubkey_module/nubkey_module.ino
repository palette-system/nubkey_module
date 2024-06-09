// Nubkeyの入力をPIM447フォーマットにしてI2Cに流す

// 開発環境の作り方
// https://ameblo.jp/pta55/entry-12654450554.html

#include <Wire.h>
#include <EEPROM.h>
#include <avr/wdt.h>
#include <avr/pgmspace.h>

// nubkey_module の製品識別番号
#define NUBKEY_SELIAL_NO  0x20

// ファームウェアのバージョン
#define NUBKEY_FIRMWARE_VAR  0x01

// ATTiny424 本体のアドレス
#define I2C_SLAVE_ADD_DEF  0x0A
#define I2C_SLAVE_ADD_SUB  0x0B

// 受け取るデータのバッファサイズ
#define GET_DATA_MAX_LENGTH  24

// Nubkey速度デフォルト
#define NUB_SPEED_X_DEFAULT  800
#define NUB_SPEED_Y_DEFAULT  600

// EEPROM に保存しているNubkey用の設定
struct nub_setting {
    uint8_t i2c_addr; // 自分のI2Cアドレス
    uint8_t nubkey_off_status; // Nubkey_offにするかどうか(0=Nubkey OFF / 1=Nubkey ON / 2=ピンの情報優先)
    short key_actuation; // Nubkey_off時のアクチュエーションポイント
    short nub_start_time; // Nubkeyとしてマウス操作させるまでの時間(ミリ秒)
    short nub_start_down; // Nubkey マウス移動開始させるアクチュエーションポイント
    short nub_speed_left; // Nubkey マウス移動させる速度 左
    short nub_speed_right; // Nubkey マウス移動させる速度 右
    short nub_speed_up; // Nubkey マウス移動させる速度 上
    short nub_speed_down; // Nubkey マウス移動させる速度 下
    short rang_x; // キャリブレーション中心の X 座標
    short rang_y; // キャリブレーション中心の Y 座標
    short loop_delay; // 読み込みサイクルのdelay値(ミリ秒)
};

// 現在の I2C の自分のアドレス
uint8_t i2c_addr;

// PIM447に合わせたデータフォーマットの場合(res_type = 0)
//   uint8_t 左 / uint8_t 右 / uint8_t 上 / uint8_t 下 / uint8_t 0x80でボタン押下
// アナログ値データフォーマットの場合(res_type = 1)
//   short X座標 / short Y座標 / short 押し込み具合
uint8_t res_data[8];

// レスポンスのタイプ
uint8_t res_type;

// 受け取ったコマンド -1=なし / 0>=受け取ったコマンド
short get_cmd;

// 受け取ったデータ
uint8_t get_data[GET_DATA_MAX_LENGTH];

// EEPROMに設定しているNubkey用の設定
nub_setting  nubst;

// Nubkeyの状態
short au, ad, al, ar; // 上下左右のホールセンサの値
short nub_x, nub_y; // NubkeyのX座標,Y座標
short nub_down; // 押し込み具合

// 各設定ピンの状態
bool nubkey_off_mode; // Nubkey off ピン
bool actuation_mode; // アクチュエーションポイント設定ピン
bool calibration_mode; // キャリブレーション設定ピン
uint8_t actuation_status; // アクチュエーションモード設定フラグ(0=アクチュエーション設定 OFF / 1=アクチュエーション設定 ON / 2=ピンの情報優先)
uint8_t calibration_status; // キャリブレーションモード設定フラグ(0=キャリブレーション OFF / 1=キャリブレーション ON / 2=ピンの情報優先)

// キャリブレーション設定用変数
short nub_x_min; // 取得できる X 座標の最小値
short nub_x_max; // 取得できる X 座標の最大値
short nub_y_min; // 取得できる Y 座標の最小値
short nub_y_max; // 取得できる Y 座標の最大値

// 前回のキーダウンステータス
uint8_t key_down_last;

// キーを押し始めた時間
unsigned long key_down_start;

// アクチュエーションポイント設定モード前回のステータス
bool actuation_mode_last;

// キャリブレーション設定モード前回のステータス
bool calibration_mode_last;

// I2Cイベント
void receiveEvent(int data_len); // データを受け取った
void requestEvent(); // データ要求を受け取った

// Nubkeyの設定初期値
void nubst_reset() {
    nubst.i2c_addr = 0; // 自分のアドレス 0 が設定されている場合はI2Cのアドレスはデフォルトの0x0A(ピンの設定によっては 0x0B)になる
    nubst.nubkey_off_status = 2; // Nubkey_offにするかどうか(0=Nubkey OFF / 1=Nubkey ON / 2=ピンの情報優先)
    nubst.key_actuation = 280; // キーをONとする閾値(Nubkey OFF モードの時に使われる)
    nubst.nub_start_time = 200; // Nubkeyとしてマウス操作させるまでの時間(ミリ秒)
    nubst.nub_start_down = 360; // Nubkeyとしてマウス操作開始させるアクチュエーションポイント
    nubst.nub_speed_left = NUB_SPEED_X_DEFAULT; // Nubkey マウス移動のスピード X（数値が低い方が早い）
    nubst.nub_speed_right = NUB_SPEED_X_DEFAULT; // Nubkey マウス移動のスピード X（数値が低い方が早い）
    nubst.nub_speed_up = NUB_SPEED_Y_DEFAULT; // Nubkey マウス移動のスピード Y (Y軸のがブレ幅が少ないのでY軸の方が早く動くようにする)
    nubst.nub_speed_down = NUB_SPEED_Y_DEFAULT; // Nubkey マウス移動のスピード Y (Y軸のがブレ幅が少ないのでY軸の方が早く動くようにする)
    nubst.rang_x = 0; // キャリブレーション X 座標
    nubst.rang_y = 0; // キャリブレーション Y 座標
    nubst.loop_delay = 2; // 読み込みサイクルのdelay(ミリ秒)
}

void setup() {
    uint8_t c;

    // Nubkeyの設定初期値
    nubst_reset();

    // 初めての起動の場合EPPROMにデフォルト設定を書き込む
    c = EEPROM.read(0); // 最初の0バイト目を読み込む
    if (c != 0x3A) {
      EEPROM.write(0, 0x3A); // 初期化したよを書き込む
      EEPROM.put(1, nubst); // 初期値を書き込んでおく
    }

    // EEPROMから設定を読み込む
    EEPROM.get(1, nubst);
    
    // Nubkey で使用するホールセンサーピン初期化
    pinMode(PIN_PA4, INPUT); // 下
    pinMode(PIN_PA5, INPUT); // 左
    pinMode(PIN_PA6, INPUT); // 上
    pinMode(PIN_PA7, INPUT); // 右

    // 設定ピン初期化
    pinMode(PIN_PA1, INPUT_PULLUP); // アドレス変更ピン
    pinMode(PIN_PA2, INPUT_PULLUP); // レスポンスタイプ変更ピン
    pinMode(PIN_PA3, INPUT_PULLUP); // キャリブレーション
    pinMode(PIN_PB2, INPUT_PULLUP); // アクチュエーション設定ボタン
    pinMode(PIN_PB3, INPUT_PULLUP); // Nubキーオフモード

    // 変数初期化
    key_down_last = 0;
    actuation_mode_last = false;
    calibration_mode_last = false;
    get_cmd = -1;
    actuation_status = 2;
    calibration_status = 2;
    nub_x_min = 0; // キャリブレーション X 座標の最小値
    nub_x_max = 0; // キャリブレーション X 座標の最大値
    nub_y_min = 0; // キャリブレーション Y 座標の最小値
    nub_y_max = 0; // キャリブレーション Y 座標の最大値
    memset(res_data, 0x00, 8);
    memset(get_data, 0x00, GET_DATA_MAX_LENGTH);

    // レスポンスタイプ    0=PIM447互換 / 1=アナログ値取得モード
    res_type = (digitalRead(PIN_PA2))? 0: 1;

    // I2C のアドレス    デフォルト=0x0A / GND=0x0B
    if (nubst.i2c_addr == 0) {
      i2c_addr = (digitalRead(PIN_PA1))? I2C_SLAVE_ADD_DEF: I2C_SLAVE_ADD_SUB;
    } else {
      i2c_addr = nubst.i2c_addr;
    }

    // I2C スレーブ初期化
    Wire.begin(i2c_addr);
    Wire.onReceive(receiveEvent);
    Wire.onRequest(requestEvent);

}

// コマンドを受け取った
void receiveEvent(int data_len) {
  int i = 0;
  // 送られてきたデータの取得
  while (Wire.available()) {
    get_data[i] = Wire.read();
    i++;
    if (GET_DATA_MAX_LENGTH <= i) break;
  }
  // バッファを上回るデータが渡されていたら空読み
  while (Wire.available()) Wire.read();
  // 受け取ったコマンドがあればコマンドを取得
  get_cmd = (i > 0)? get_data[0]: -1;
}

// I2Cデータ要求を受け取った時の処理
void requestEvent() {
  short c, d, e, f;

  if (get_cmd == 0x40) {
    // アドレス設定
    nubst.i2c_addr = get_data[1];
    // EEPROMに保存
    EEPROM.put(1, nubst);
    // コマンドをエコー
    Wire.write(get_cmd);
    // 60ミリ秒後に再起動
    wdt_enable(WDTO_60MS);

  } else if (get_cmd == 0x41) {
    // 設定情報取得コマンドを受け取った場合は設定情報を返す
    Wire.write(get_cmd); // コマンドをエコー
    Wire.write(nubst.i2c_addr & 0xFF);
    Wire.write(nubst.nubkey_off_status & 0xFF);
    Wire.write((nubst.key_actuation >> 8) & 0xFF);
    Wire.write(nubst.key_actuation & 0xFF);
    Wire.write((nubst.nub_start_time >> 8) & 0xFF);
    Wire.write(nubst.nub_start_time & 0xFF);
    Wire.write((nubst.nub_start_down >> 8) & 0xFF);
    Wire.write(nubst.nub_start_down & 0xFF);
    Wire.write((nubst.nub_speed_left >> 8) & 0xFF);
    Wire.write(nubst.nub_speed_left & 0xFF);
    Wire.write((nubst.nub_speed_right >> 8) & 0xFF);
    Wire.write(nubst.nub_speed_right & 0xFF);
    Wire.write((nubst.nub_speed_up >> 8) & 0xFF);
    Wire.write(nubst.nub_speed_up & 0xFF);
    Wire.write((nubst.nub_speed_down >> 8) & 0xFF);
    Wire.write(nubst.nub_speed_down & 0xFF);
    Wire.write((nubst.rang_x >> 8) & 0xFF);
    Wire.write(nubst.rang_x & 0xFF);
    Wire.write((nubst.rang_y >> 8) & 0xFF);
    Wire.write(nubst.rang_y & 0xFF);
    Wire.write((nubst.loop_delay >> 8) & 0xFF);
    Wire.write(nubst.loop_delay & 0xFF);

  } else if (get_cmd == 0x42) {
    // Nubkey OFFモードの設定を更新してEEPROMに保存
    c = nubst.nubkey_off_status;
    nubst.nubkey_off_status = get_data[1];
    // 変更があればEEPROMに保存
    if (c != nubst.nubkey_off_status) EEPROM.put(1, nubst);
    EEPROM.put(1, nubst);
    // コマンドをエコー
    Wire.write(get_cmd);

  } else if (get_cmd == 0x43) {
    // アクチュエーション設定モードの設定を更新
    actuation_status = get_data[1];
    // コマンドをエコー
    Wire.write(get_cmd);

  } else if (get_cmd == 0x44) {
    // キャリブレーション設定モードの設定を更新
    calibration_status = get_data[1];
    // コマンドをエコー
    Wire.write(get_cmd);

  } else if (get_cmd == 0x45) {
    // アクチュエーションポイント設定を更新してEEPROMに保存
    c = nubst.key_actuation;
    nubst.key_actuation = (get_data[1] << 8) | get_data[2];
    // 変更があればEEPROMに保存
    if (c != nubst.key_actuation) EEPROM.put(1, nubst);
    EEPROM.put(1, nubst);
    // コマンドをエコー
    Wire.write(get_cmd);

  } else if (get_cmd == 0x46) {
    // Nubkeyとしてマウス操作させるまでの時間を更新してEEPROMに保存
    c = nubst.nub_start_time;
    nubst.nub_start_time = (get_data[1] << 8) | get_data[2];
    // 変更があればEEPROMに保存
    if (c != nubst.nub_start_time) EEPROM.put(1, nubst);
    // コマンドをエコー
    Wire.write(get_cmd);

  } else if (get_cmd == 0x47) {
    // Nubkey 動かし始める高さを変更してEEPROMに保存
    c = nubst.nub_start_down;
    nubst.nub_start_down = (get_data[1] << 8) | get_data[2];
    // 変更があればEEPROMに保存
    if (c != nubst.nub_start_down) EEPROM.put(1, nubst);
    // コマンドをエコー
    Wire.write(get_cmd);

  } else if (get_cmd == 0x48) {
    // Nubkey マウス移動のスピードを更新してEEPROMに保存
    c = nubst.nub_speed_left;
    d = nubst.nub_speed_right;
    e = nubst.nub_speed_up;
    f = nubst.nub_speed_down;
    nubst.nub_speed_left = (get_data[1] << 8) | get_data[2];
    nubst.nub_speed_right = (get_data[3] << 8) | get_data[4];
    nubst.nub_speed_up = (get_data[5] << 8) | get_data[6];
    nubst.nub_speed_down = (get_data[7] << 8) | get_data[8];
    // 変更があればEEPROMに保存
    if (c != nubst.nub_speed_left || 
        d != nubst.nub_speed_right || 
        e != nubst.nub_speed_up || 
        f != nubst.nub_speed_down) EEPROM.put(1, nubst);
    // コマンドをエコー
    Wire.write(get_cmd);

  } else if (get_cmd == 0x49) {
    // Nubkey 読み込みサイクルのdelay値を変更してEEPROMに保存
    c = nubst.loop_delay;
    nubst.loop_delay = (get_data[1] << 8) | get_data[2];
    // 変更があればEEPROMに保存
    if (c != nubst.loop_delay) EEPROM.put(1, nubst);
    // コマンドをエコー
    Wire.write(get_cmd);

  } else if (get_cmd == 0x4A) {
    // 設定をすべてリセット
    nubst_reset();
    // EEPROMに保存
    EEPROM.put(1, nubst);
    // コマンドをエコー
    Wire.write(get_cmd);
    // 60ミリ秒後に再起動
    wdt_enable(WDTO_60MS);

  } else if (get_cmd == 0x50) {
    // アナログ値要求
    Wire.write(get_cmd);
    Wire.write((nub_x >> 8) & 0xFF);
    Wire.write(nub_x & 0xFF);
    Wire.write((nub_y >> 8) & 0xFF);
    Wire.write(nub_y & 0xFF);
    Wire.write((nub_down >> 8) & 0xFF);
    Wire.write(nub_down & 0xFF);

  } else if (get_cmd == 0x51) {
    // アナログ値要求上下左右の順番
    Wire.write(get_cmd);
    Wire.write((au >> 8) & 0xFF);
    Wire.write(au & 0xFF);
    Wire.write((ad >> 8) & 0xFF);
    Wire.write(ad & 0xFF);
    Wire.write((al >> 8) & 0xFF);
    Wire.write(al & 0xFF);
    Wire.write((ar >> 8) & 0xFF);
    Wire.write(ar & 0xFF);

  } else if (get_cmd == 0x60) {
    // コマンド 0x60 を受け取った場合は自分のアドレスと nubkey_module と判定するための番号を返す
    Wire.write(get_cmd);
    Wire.write(i2c_addr);
    Wire.write(NUBKEY_SELIAL_NO);
    Wire.write(NUBKEY_FIRMWARE_VAR);

  } else if (res_type == 1) {
    // アナログ値を返す short X / short Y / short 押し込み の6byte
    Wire.write(res_data, 6);

  } else {
    // デフォルトではPIM447フォーマット 5byte
    Wire.write(res_data, 5);
    // キーダウンは一度送信したら終わり
    if (!nubkey_off_mode) res_data[4] = 0x00;
  }
  // 呼ばれたコマンドをリセット
  get_cmd = -1;
}


void loop() {
  short mx, my;
  unsigned long t, n, p;

  // Nubkeyオフモードかピンの状態を取得
  if ((nubst.nubkey_off_status == 2 && !digitalRead(PIN_PB3)) || nubst.nubkey_off_status == 1) {
    nubkey_off_mode = true;
  } else {
    nubkey_off_mode = false;
  }

  // アクチュエーションポイント設定モードかピンの状態を取得
  if ((actuation_status == 2 && !digitalRead(PIN_PB2)) || actuation_status == 1) {
    actuation_mode = true;
  } else {
    actuation_mode = false;
  }

  // キャリブレーション設定モードかピンの状態を取得
  if ((calibration_status == 2 && !digitalRead(PIN_PA3)) || calibration_status == 1) {
    calibration_mode = true;
  } else {
    calibration_mode = false;
  }

  // Nubkey の値取得
  au = analogRead(PIN_PA6); // 上
  ad = analogRead(PIN_PA4); // 下
  ar = analogRead(PIN_PA7); // 右
  al = analogRead(PIN_PA5); // 左
  nub_x = al - ar;
  nub_y = au - ad;
  nub_down = ((au + ad + ar + al) / 4);

  if (actuation_mode) {
    // アクチュエーションポイント設定モード開始
    actuation_mode_last = true;

  } else if (!actuation_mode && actuation_mode_last) {
    // アクチュエーションポイント設定モード 終了時
    actuation_mode_last = false;
    // 今のキーの高さをアクチュエーションポイントに設定
    nubst.key_actuation = nub_down;
    // EEPROMに保存
    EEPROM.put(1, nubst);

  } else if (calibration_mode) {
    // キャリブレーション設定モード 開始時
    if (calibration_mode_last == false) {
      // キャリブレーション設定開始時に値をリセット
      nub_x_min = nub_x_max = nub_y_min = nub_y_max = 0;
    }
    calibration_mode_last = true;
    // 現在のXYを設定バッファに書き込み
    if (nub_x_min > nub_x) nub_x_min = nub_x;
    if (nub_x_max < nub_x) nub_x_max = nub_x;
    if (nub_y_min > nub_y) nub_y_min = nub_y;
    if (nub_y_max < nub_y) nub_y_max = nub_y;

  } else if (!calibration_mode && calibration_mode_last) {
    // キャリブレーション設定モード 終了時
    calibration_mode_last = false;
    // ふり幅が40以上の場合のみ設定変更(40以下は触られていないとする)
    mx = nub_x_max - nub_x_min;
    my = nub_y_max - nub_y_min;
    if (mx > 40 && my > 40) {
      nubst.rang_x = nub_x_min + (mx / 2);
      nubst.rang_y = nub_y_min + (my / 2);
      // EEPROMに保存
      EEPROM.put(1, nubst);
    }

  } else if (res_type == 1) {
    // レスポンスタイプ アナログ値 6byte
    // short X / short Y / short 押し込み
    res_data[0] = ((nub_x >> 8) & 0xFF);
    res_data[1] = (nub_x & 0xFF);
    res_data[2] = ((nub_y >> 8) & 0xFF);
    res_data[3] = (nub_y & 0xFF);
    res_data[4] = ((nub_down >> 8) & 0xFF);
    res_data[5] = (nub_down & 0xFF);

  } else {
    // 【デフォルト】
    // レスポンスタイプ PIM447フォーマット 5byte
    // uint8_t 左 / uint8_t 右 / uint8_t 上 / uint8_t 下 / uint8_t 0x80でボタン押下

    if (nubkey_off_mode) {
      // Nubkey オフモードの場合はスイッチのON/OFFのみ返す
      memset(res_data, 0x00, 4); // 上下左右を0クリア
      res_data[4] = (nub_down < nubst.key_actuation)? 0x80 : 0x00; // ボタンの情報を渡す

    } else {
      // Nubkeyの動作
      n = millis(); // 今の時間
      memset(res_data, 0x00, 4); // 上下左右を0クリア
      if (nub_down < nubst.nub_start_down) {
        if (key_down_last == 0) {
          // キー押し始め最初の処理
          key_down_last = 1;
          key_down_start = millis();
        } else {
          // キー押し続けられてる
          // どれくらい押されていたか計算
          t = n - key_down_start;
          // Nubkey開始時間より長く押されていたらマウス移動
          if (t > (unsigned long)nubst.nub_start_time) {
            mx = nub_x - nubst.rang_x; // キャリブレーションの位置を反映
            my = nub_y - nubst.rang_y; // キャリブレーションの位置を反映
            p = (nubst.nub_start_down - nub_down) / 4; // 押し込み具合取得
            // noInterrupts(); // 割り込み禁止 開始
            if (mx < 0) {
              res_data[0] = (int)(abs(mx) * p / nubst.nub_speed_left) & 0xFF;
            } else {
              res_data[1] = (int)(abs(mx) * p / nubst.nub_speed_right) & 0xFF;
            }
            if (my < 0) {
              res_data[2] = (int)(abs(my) * p / nubst.nub_speed_up) & 0xFF;
            } else {
              res_data[3] = (int)(abs(my) * p / nubst.nub_speed_down) & 0xFF;
            }
            // interrupts(); // 割り込み禁止 解除
          }
        }
      } else if (key_down_last == 1) {
          // キー離された
          key_down_last = 0;
          // どれくらい押されていたか計算
          t = n - key_down_start;
          // Nubkey開始前に離されたならばKeyダウンを送信
          if (t < (unsigned long)nubst.nub_start_time) res_data[4] = 0x80;

      }
    }
  }

  delay(nubst.loop_delay);
}
