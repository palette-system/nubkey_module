// Nubkeyの入力をPIM447フォーマットにしてI2Cに流す

// 開発環境の作り方
// https://ameblo.jp/pta55/entry-12654450554.html

#include <Wire.h>
#include <EEPROM.h>
#include <avr/wdt.h>
#include <avr/pgmspace.h>


// ATTiny424 本体のアドレス
#define I2C_SLAVE_ADD_DEF  0x0A
#define I2C_SLAVE_ADD_SUB  0x0B

// 受け取るデータのバッファサイズ
#define GET_DATA_MAX_LENGTH  24

// EEPROM に保存しているNubkey用の設定
struct nub_setting {
    uint8_t i2c_addr; // 自分のI2Cアドレス
    short key_actuation; // キーをONとする閾値
    unsigned short nub_start_time; // Nubkeyとしてマウス操作させるまでの時間(ミリ秒)
    short nub_speed_x; // Nubkey マウス移動させる速度 X
    short nub_speed_y; // Nubkey マウス移動させる速度 Y
    short rang_x; // キャリブレーション中心の X 座標
    short rang_y; // キャリブレーション中心の Y 座標
};

// I2C の自分のアドレス
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

void setup() {
    uint8_t c;

    // Nubkeyの設定初期値
    nubst.i2c_addr = 0; // 自分のアドレス 0 が設定されている場合はI2Cのアドレスはデフォルトの0x0A(ピンの設定によっては 0x0B)になる
    nubst.key_actuation = 280; // キーをONとする閾値(Nubkey OFF モードの時に使われる)
    nubst.nub_start_time = 200; // Nubkeyとしてマウス操作させるまでの時間(ミリ秒)
    nubst.nub_speed_x = 1500; // Nubkey マウス移動のスピード X（数値が低い方が早い）
    nubst.nub_speed_y = 1100; // Nubkey マウス移動のスピード Y (Y軸のがブレ幅が少ないのでY軸の方が早く動くようにする)
    nubst.rang_x = 0; // キャリブレーション X 座標
    nubst.rang_y = 0; // キャリブレーション Y 座標

    // 初めての起動の場合EPPROMにデフォルト設定を書き込む
    c = EEPROM.read(0); // 最初の0バイト目を読み込む
    if (c != 0x33) {
      EEPROM.write(0, 0x33); // 初期化したよを書き込む
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
    nub_x_min = 0; // キャリブレーション X 座標の最小値
    nub_x_max = 0; // キャリブレーション X 座標の最大値
    nub_y_min = 0; // キャリブレーション Y 座標の最小値
    nub_y_max = 0; // キャリブレーション Y 座標の最大値
    memset(res_data, 0x00, 8);
    memset(get_data, 0x00, GET_DATA_MAX_LENGTH);

    // レスポンスタイプ    0=PIM447互換 / 1=アナログ値取得モード / 2=
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
  if (get_cmd == 0x00) {
    // コマンド 0x00 を受け取った場合は自分のアドレスをエコー
    Wire.write(i2c_addr);

  } else if (get_cmd == 0x40) {
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
    Wire.write((nubst.key_actuation >> 8) & 0xFF);
    Wire.write(nubst.key_actuation & 0xFF);
    Wire.write((nubst.nub_start_time >> 8) & 0xFF);
    Wire.write(nubst.nub_start_time & 0xFF);
    Wire.write((nubst.nub_speed_x >> 8) & 0xFF);
    Wire.write(nubst.nub_speed_x & 0xFF);
    Wire.write((nubst.nub_speed_y >> 8) & 0xFF);
    Wire.write(nubst.nub_speed_y & 0xFF);
    Wire.write((nubst.rang_x >> 8) & 0xFF);
    Wire.write(nubst.rang_x & 0xFF);
    Wire.write((nubst.rang_y >> 8) & 0xFF);
    Wire.write(nubst.rang_y & 0xFF);

  } else if (get_cmd == 0x42) {
    // アクチュエーションポイント設定を更新してEEPROMに保存
    nubst.key_actuation = (get_data[1] << 8) | get_data[2];
    // EEPROMに保存
    EEPROM.put(1, nubst);
    // コマンドをエコー
    Wire.write(get_cmd);

  } else if (get_cmd == 0x43) {
    // Nubkeyとしてマウス操作させるまでの時間を更新してEEPROMに保存
    nubst.nub_start_time = (get_data[1] << 8) | get_data[2];
    // EEPROMに保存
    EEPROM.put(1, nubst);
    // コマンドをエコー
    Wire.write(get_cmd);

  } else if (get_cmd == 0x44) {
    // Nubkey マウス移動のスピードを更新してEEPROMに保存
    nubst.nub_speed_x = (get_data[1] << 8) | get_data[2];
    nubst.nub_speed_y = (get_data[3] << 8) | get_data[4];
    // EEPROMに保存
    EEPROM.put(1, nubst);
    // コマンドをエコー
    Wire.write(get_cmd);

  } else if (get_cmd == 0x45) {
    // 設定をすべてリセット
    nubst.i2c_addr = 0;
    nubst.key_actuation = 280;
    nubst.nub_start_time = 200;
    nubst.nub_speed_x = 1500;
    nubst.nub_speed_y = 1100;
    nubst.rang_x = 0;
    nubst.rang_y = 0;
    // EEPROMに保存
    EEPROM.put(1, nubst);
    // コマンドをエコー
    Wire.write(get_cmd);
    // 60ミリ秒後に再起動
    wdt_enable(WDTO_60MS);

  } else if (get_cmd == 0x50) {
    // アナログ値要求
    Wire.write((nub_x >> 8) & 0xFF);
    Wire.write(nub_x & 0xFF);
    Wire.write((nub_y >> 8) & 0xFF);
    Wire.write(nub_y & 0xFF);
    Wire.write((nub_down >> 8) & 0xFF);
    Wire.write(nub_down & 0xFF);

  } else if (get_cmd == 0x51) {
    // アナログ値要求上下左右の順番
    Wire.write((au >> 8) & 0xFF);
    Wire.write(au & 0xFF);
    Wire.write((ad >> 8) & 0xFF);
    Wire.write(ad & 0xFF);
    Wire.write((al >> 8) & 0xFF);
    Wire.write(al & 0xFF);
    Wire.write((ar >> 8) & 0xFF);
    Wire.write(ar & 0xFF);

  } else if (res_type == 1) {
    // アナログ値を返す short X / short Y / short 押し込み の6byte
    Wire.write(res_data, 6);

  } else {
    // デフォルトではPIM447フォーマット 5byte
    Wire.write(res_data, 5);
    // キーダウンは一度送信したら終わり
    res_data[4] = 0x00;
  }
  // 呼ばれたコマンドをリセット
  get_cmd = -1;
}


void loop() {
  short mx, my;
  unsigned long t, n, p;
  bool nubkey_off_mode;
  bool actuation_mode;
  bool calibration_mode;

  // Nubkeyオフモードかピンの状態を取得
  nubkey_off_mode = (bool)!digitalRead(PIN_PB3);

  // アクチュエーションポイント設定モードかピンの状態を取得
  actuation_mode = (bool)!digitalRead(PIN_PB2);

  // キャリブレーション設定モードかピンの状態を取得
  calibration_mode = (bool)!digitalRead(PIN_PA3);

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
    // キャリブレーション設定モード
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
    // ふり幅が60以上の場合のみ設定変更(40以下は触られていないとする)
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
      if (nub_down < 360) {
        if (key_down_last == 0) {
          // キー押し始め最初の処理
          key_down_last = 1;
          key_down_start = millis();
        } else {
          // キー押し続けられてる
          // どれくらい押されていたか計算
          t = n - key_down_start;
          // Nubkey開始時間より長く押されていたらマウス移動
          mx = nub_x - nubst.rang_x; // キャリブレーションの位置を反映
          my = nub_y - nubst.rang_y; // キャリブレーションの位置を反映
          p = (360 - nub_down);
          if (t > nubst.nub_start_time) {
            if (mx < 0) {
              res_data[0] = abs(mx) * p / nubst.nub_speed_x;
            } else {
              res_data[1] = abs(mx) * p / nubst.nub_speed_x;
            }
            if (my < 0) {
              res_data[2] = abs(my) * p / nubst.nub_speed_y;
            } else {
              res_data[3] = abs(my) * p / nubst.nub_speed_y;
            }
          }
        }
      } else if (key_down_last == 1) {
          // キー離された
          key_down_last = 0;
          // どれくらい押されていたか計算
          t = n - key_down_start;
          // Nubkey開始前に離されたならばKeyダウンを送信
          if (t < nubst.nub_start_time) res_data[4] = 0x80;

      }
    }

  }

  delay(2);
}
