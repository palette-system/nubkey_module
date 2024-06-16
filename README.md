# nubkey_module
Nubkey をPIM447と同じ通信で使用できるようにしたモジュールです。

<br><br>

<table>
<tr><td><img src="/images/pcb_top.png" width="400"></td><td><img src="/images/pcb_back.png" width="400"></td></tr>
</table>

<br><br>

## ドキュメント

<a href="/docs/structure.md">nubkey_module　基礎知識</a><br>
nubkey_modul を使用するにあたり作成者も、設計者も一度確認してからご使用ください。<br>
<br>

<a href="/docs/module_build.md">nubkey_module　はんだ付け手順</a><br>
nubkey_module をPCBにはんだ付けする際にご確認下さい。<br>
<br>

<a href="/docs/tester_build.md">nubkey_module tester　ビルドガイド</a><br>
nubkey_module tester を組み立てる際にご確認下さい。<br>
<br>

<a href="https://palette-system.github.io/nubkey_module/">nubkey_module tester　ファームウェア書き込み</a><br>
nubkey_module tester を使用する際にご確認下さい。<br>
<br>

<a href="/docs/command.md">nubkey_module　通信仕様</a><br>
nubkey_module で実行できるI2Cのコマンドの仕様書です。QMK等から速度調節する場合等にご確認下さい。<br>
<br>

<a href="/hardware/nubkey_module_tester/">nubkey_module tester　KiCadデータ</a><br>
nubkey_module tester のKiCadデータです。nubkey_module を使用したキーボードを作る際の参考にして下さい。<br>
<br>
<br>

## 基本的な接続
<br>
<img src="/images/haisen_1.png" width="700"><br>
<br>

## キャリブレーション
どんなに気を付けて作成しても組み立て毎に磁気スイッチの中心位置が0.数ミリズレが生じます。<br>
キャリブレーションスイッチを押したまま、Nubkeyを押下して上下左右にグリグリ動かす事で nubkey_module のソフト側で中心位置を調節できます。<br>
<br>

## オススメの構造
<br>
<img src="/images/danmen_1.png" width="700"><br>
<br>

<img src="/images/ok_2.png" width="700"><br>
<br>

## ピンの説明

|  ピン  |  説明  |
|  ----  |  :---  |
|  UPDI  |  nubkey_module のファームを更新するためのピンです  |
|  addr 0x0B  |  nubkey_module のI2Cアドレスを0x0Bにするピンです。<br>このピンをGNDに接続した状態で起動するとI2Cアドレスが0x0Bになります。  |
|  raw res  |  I2CのレスポンスがデフォルトではPIM447互換ですがこのピンをGNDに接続すると「short X座標, short Y座標, short スイッチの押し込み具合」を返すようになります。  |
|  actuation  |  アクチュエーションポイント設定ピンです。このピンをGNDに接続するとアクチュエーション設定モードに入ります。その後GNDへの接続を切断したタイミングのKS-20のスイッチの高さがアクチュエーションポイントとしてnubkey_moduleに保存されます。  |
|  nubkey off  |  このピンをGNDに接続すると、マウス移動が無しの状態になりスイッチのON/OFFのみが変わるようになります。この時actuationピンで設定したスイッチの高さでON/OFFが切り替わります。  |
|  GND  |  GNDです  |
|  VCC  |  電源です。3.3Vを接続して下さい。  |
|  SCL  |  I2C通信用（SCL）  |
|  SDA  |  I2C通信用（SDA）  |
|  calibration  |  キャリブレーションピンです。このピンをGNDに接続した状態でKS-20のスイッチを押してグリグリすると、Nubkeyの中心位置を調節できます。<br>キャリブレーションの結果は nubkey_module に保存されるので起動のたびキャリブレーションを行う必要はありません。  |




