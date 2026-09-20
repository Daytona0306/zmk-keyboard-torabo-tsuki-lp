# torabo-tsuki LP 用 ZMK ファームウェア（DYA Studio 構成）

[torabo-tsuki LP](https://github.com/sekigon-gonnoc/torabo-tsuki-lp) 用の ZMK ファームウェアです。
本家 [sekigon-gonnoc/zmk-keyboard-torabo-tsuki-lp](https://github.com/sekigon-gonnoc/zmk-keyboard-torabo-tsuki-lp)
の `v0.3+dya-studio` ブランチをベースに、ZMK v0.4 系へ移行して DYA Studio の機能を一通り有効化し、
トラックボール周りを拡張したものです。

* `_central` がついている uf2 をトラックボールがついている方に、`_peripheral` を反対側に書き込んでください
* キーマップは keymap-editor および ZMK Studio / DYA Studio で編集できます

---

未着手・未確認の事項は [TODO.md](TODO.md) に分けてあります。

## ブランチ運用

**焼く uf2 は必ず `main` の artifacts から取ってください。**

| ブランチ | リビジョン | 役割 |
|---|---|---|
| **`main`** | **SHA 固定** | **常用。いまキーボードに焼いてあるもの。**焼く uf2 は必ずここの artifacts から取る |
| **`update-modules`** | 浮動 | 上流モジュールへの追随を試すところ。CI が緑なら `main` の SHA を進める |
| 実験用（都度作る） | `main` を継ぐ | 試したい設定。良ければ `main` にマージ、駄目ならブランチごと削除 |
| `master` / `ZMK_Latest` / `v0.3+*` / `v0.4+*` | — | 過去の経緯。`master` は upstream 取り込み用に残しているだけで、2026-03 で止まっており `main` の祖先ではない |

### なぜ `main` を固定するのか

`main` は「常に最新」ではなく「**いま焼いてあるもの**」です。それが再現できないと意味がありません。

* GitHub Actions の **artifact は 90 日で消える**
* ファームには config のバージョンが埋まらない（下記）

この2つが重なると、浮動のままでは「いま快調に使っているファーム」を**二度と作れません。**
同じコミットをビルドしても、上流モジュールが動いていれば別物が出てきます。

**実際に一度やられています。** `9a72f74` は、自分では何も変えていないのに
`error: too many arguments to function 'zmk_keymap_layer_activate'` でビルドが落ちたときのものです。
cormoran さんが各モジュールを ZMK v0.4 へ移行させたのが `revision: main` 経由で降ってきていました。
このときの固定は v0.4 移行のときに失われ、29 プロジェクト全部が浮動に戻っていました。

### 更新のしかた

```sh
tools/west-pins.py            # 追跡先の先頭に固定し直す
tools/west-pins.py --unpin    # ブランチ名に戻す
tools/west-pins.py --check    # 追跡先が進んでいるか見るだけ（書き換えない）
```

固定した行には `# track: <ref>` が付きます。どのブランチ由来かはこれで分かり、
`--unpin` と次回の固定はこのコメントを読みます。コメントを壊さないよう
YAML として読み書きせず行単位で書き換えています。

上流に追随したくなったら:

```sh
git checkout -B update-modules main
tools/west-pins.py --unpin && git commit -am "Unpin for an upstream check"
git push -f -u origin update-modules       # CI が回る
# 緑なら main 側で
tools/west-pins.py && git commit -am "Move pins to current upstream"
```

**焼いて問題なければ `main` を進める**、という順序は変わりません。

**ファームウェアには config リポジトリのバージョンが埋まりません。**
ビルド時の `ZMK_CONFIG_BUILD_VERSION` は空です（ワークフローが config を git ではないコピーに置くため）。
`ZMK_BUILD_VERSION` は ZMK 本体の版で、このリポジトリとは無関係です。

つまり **どのブランチのファームを焼いたかは、デバイス側から確認できません。**
実験ブランチを焼いて気に入ったら、**その場で `main` に進めておくこと。**
後から「どれを焼いたんだったか」を突き止める手段はありません。

---

## 本家からの主な変更点

### 1. ZMK 本体を v0.4 系へ

| | 本家 `v0.3+dya-studio` | 本構成 |
|---|---|---|
| ZMK | `cormoran/zmk` @ `v0.3-branch+dya` | **`cormoran/zmk` @ `main+dya`** |
| Zephyr | 3.5 | **4.1** |
| ボード | `zmk-component-bmp-boost` @ `v0.2`（HWMv1） | **`master`（HWMv2）** |
| ワークフロー | `build-user-config.yml@v0.3` | **`@main`** |

DYA Studio の**ランタイムマクロ・ランタイムコンボは cormoran 版 ZMK の custom Studio RPC が前提**で、
これは v0.4 系にしかありません。v0.3 系のままでは Studio 上で「サブシステムが見つかりません」と表示されます。

Zephyr 4.1 は HWMv2（`board.yml` を持つ新しいボード定義形式）を要求するため、ボードも `master` に切り替えています。
`v0.2` ブランチは `boards/arm/` 配下の旧形式で、Zephyr 4.1 では使えません。

### 2. DYA Studio の機能を有効化

| モジュール | Studio での機能 |
|---|---|
| `zmk-feature-custom-settings` | 設定の永続化レイヤ（下記 runtime 系の土台） |
| `zmk-feature-runtime-macro` | マクロをランタイムで作成・編集（`&rmacro <slot>`） |
| `zmk-feature-runtime-combo` | コンボをランタイムで作成・編集 |
| `zmk-module-settings-rpc` | 各種設定の RPC |
| `zmk-module-ble-management` | Bluetooth 接続管理 |
| `zmk-module-runtime-input-processor` | トラックボール設定のランタイム変更 |
| `zmk-feature-kscan-diagnostics` | キースイッチの押下統計・チャタリング検出 |
| `zmk-feature-input-stream` | 上記のライブ表示 |
| `zmk-feature-watchdog` | フリーズ・ハードフォルト・予期しないリセットの記録 |
| `zmk-feature-device-info` | デバイス情報 |
| `zmk-module-devtool` | スタック使用量 |
| `zmk-feature-zephyr-setting-expose` | Zephyr settings の閲覧 |
| `zmk-feature-studio-rpc-perf` | Studio RPC の性能計測 |

これらの CONFIG は `snippets/split-central/split-central.conf` にまとめています。
Studio RPC は central 側でしか動かないためです。

### 3. トラックボール周りの拡張

| モジュール | 用途 |
|---|---|
| `kot149/zmk-scroll-snap` | スクロール方向を軸にスナップ |
| `shakushakupanda/zmk-mouse-gesture` | マウスジェスチャー（kot149 版のフォーク） |
| `shakushakupanda/zmk-module-mouse-gesture-rpc` | ジェスチャーを Flash に保存し Web UI から編集 |
| `mjmjm0101/zmk-input-processor-scroll-inertia` | 慣性スクロール |
| `ssbb/zmk-listeners` | レイヤーリスナー |

`zmk-mouse-gesture` が本家 kot149 ではなくフォークなのは、ZMK v0.4 の endpoints API 改名に対応しており、
ジェスチャー入力中にカーソルが動くのを抑制する機能が入っているためです。
DT プロパティと Kconfig は kot149 版から変わっていないので、`config/gestures.dtsi` はそのまま使えます。

### 4. BLE の安定化設定

長時間使用後に接続が切れる事象への対策として、左右の conf で以下を有効にしています。

```conf
CONFIG_ZMK_BLE_EXPERIMENTAL_CONN=y
CONFIG_BT_CTLR_PHY_2M=n              # 1Mbps 固定。混雑した環境での安定性を優先
CONFIG_BT_GATT_ENFORCE_SUBSCRIPTION=n # 再接続時に CCC 購読が復元されない場合の対策
```

`ENFORCE_SUBSCRIPTION=n` は、ホスト側が再接続時に購読状態を復元しないと
「接続されているのに入力が通らない」状態になるのを避けるためのものです。

### 5. Zephyr 4.1 対応のための修正

* `src/board.c` — `INPUT_CALLBACK_DEFINE` が `user_data` 引数を取るようになり、コールバックも
  `(struct input_event *evt, void *user_data)` の2引数に変更されたため追従
* `Kconfig`（リポジトリ直下）— 下記「ハマりどころ」参照

### 6. 接続先 / OS ごとのベースレイヤー

`cormoran/zmk-feature-default-layer` + `cormoran/zmk-feature-os-detection` を追加し、
USB / BLE プロファイルごと、および検出した OS ごとに常時 ON のレイヤーを切り替えられるようにしました。
DYA Studio の「OS ごとのデフォルトレイヤー」パネルから設定します。

iPad 用に **index 1 の `iPad` レイヤー**を用意しています。Windows 版（レイヤー0）との差分は2点だけです。

| | Windows（レイヤー0） | iPad（レイヤー1） |
|---|---|---|
| 修飾キー | Ctrl / Cmd | **入れ替え**（コピペ等を Cmd 側へ） |
| IME 切替 | 無変換 / 変換 | **英数（LANG2）/ かな（LANG1）** |

このモジュールはレイヤー0を落とさずに上へ重ねる作りなので、iPad レイヤーは**差分だけ**書いてあり、
残りはすべて `&trans` でレイヤー0に落ちます。

**置き場所は index 1 でなければいけません**（下記「ハマりどころ」参照）。

`default-layer` 抜きで `os-detection` 単体でも意味があります。Studio から OS 判定の結果を確認できるので、
BLE が不安定なときに「OS 誤判定なのかどうか」の切り分けに使えます。

---

## レイヤー構成

| index | ノード | Studio 表示名 | 用途 |
|---|---|---|---|
| 0 | `default_layer` | Base (Win) | ベース（Windows） |
| 1 | `iPad` | iPad | ベース（iPad / iOS）。`default-layer` が OS 判定で有効化 |
| 2 | `NUM,SYM1` | NUM/SYM1 | 数字・記号 |
| 3 | `SYM2` | SYM2 | 記号2 |
| 4 | `MOUSE` | Mouse | マウス |
| 5 | `NUM-SCROLL` | Scroll | スクロール（慣性・スナップの対象レイヤー） |
| 6 | `bluetooth` | Bluetooth | BLE プロファイル操作 |
| 7〜12 | `gest_*` | Gest: … | ジェスチャー用（下記） |

ベースレイヤーが 0〜1、機能レイヤーが 2〜6、ジェスチャーが 7〜12 と連続するように並べています。

**レイヤー番号は複数のファイルに散らばっています。**順序を変えるときは以下すべてを追従させてください。

* `config/keymap.keymap` の `&lt` / `&mo` / `&to`
* `config/gestures.dtsi` の `GESTURE_ROUTE(N, ...)`
* `boards/shields/torabo_tsuki_lp/torabo_tsuki_lp_right.overlay` の
  `active-layers = <BIT(N)>` と慣性スクロールの `layer`
* `snippets/split-central/split-central.conf` の `CONFIG_ZMK_DEFAULT_LAYER_MIN/MAX_INDEX`

---

## ジェスチャーの構成

2系統が併存しています。

### DT 定義（`config/gestures.dtsi`）

レイヤーごとに専用のプロセッサを置き、レイヤー移動でセットを切り替えます。

| レイヤー | セット | 用途 |
|---|---|---|
| 7 | `arrow_gest_proc` | 矢印キー |
| 8 | `tab_gest_proc` | タブ操作 |
| 9 | `vdesktop_gest_proc` | 仮想デスクトップ |
| 10 | `nav_gest_proc` | 戻る/進む/上の階層 |
| 11 | `win_gest_proc` | ウィンドウ管理 |
| 12 | `powertoys_gest_proc` | PowerToys |

**セットごとに効き方を変えられる**のが利点です。たとえば矢印は `stroke-size = <15>` / `idle-timeout-ms = <50>` と
機敏に、PowerToys は `stroke-size = <25>` と大きめにして誤爆を防いでいます。変更には再ビルドが必要です。

### RPC 定義（`&mg_set`）

`rpc_mouse_gesture` ノードが1つあり、Web UI からジェスチャーを登録して Flash に保存します。
`&mg_set N`（N は 0〜4）を押している間だけ有効になり、レイヤーに依存しません。**再ビルド不要**で編集できます。

保存できるジェスチャーは合計32個まで（proto 由来の上限）。設定（`stroke_size` など）はこのノード1つに
対する単一の値で、セットごとには分けられません。

**実質的に使えるセットは3つです。** `CONFIG_ZMK_MOUSE_GESTURE_RPC_NUM_SETS` は 1〜16 を受け付けますが、
モジュール側の3箇所がいずれも3固定で、Kconfig の値だけが浮いています。

| 箇所 | 実装 |
|---|---|
| Web UI のキータブ | `web/src/App.tsx` の `const NUM_GESTURE_KEYS = 3;` |
| behavior のパラメータ一覧 | `mg_set_param_values[]` が Set 0/1/2 の3件 |
| セット番号のエイリアス解析 | `binding_set_id()` の `if (c >= '0' && c <= '2')` |

セット数を firmware から UI へ伝える proto フィールドが無いため、UI 側は問い合わせようがなく定数で決め打ちです。
4つ以上使いたい場合は UI の Export した JSON に `"setId": 3` を手で足して Import するか、
UI をフォークして `NUM_GESTURE_KEYS` を変えて自前ビルドすることになります。

---

## ハマりどころ

同じ轍を踏まないための記録です。

### `&mg` をメインの input-processors チェーンに入れてはいけない

`zmk-mouse-gesture` の処理関数は、ジェスチャーが非アクティブかどうかを見ずに全ての REL X/Y を処理し、
戻り値は**グローバル変数**だけで決まります。

```c
return g_suppress_cursor ? ZMK_INPUT_PROC_STOP : ZMK_INPUT_PROC_CONTINUE;
```

`g_suppress_cursor` は `&mouse_gesture_on` でも立つため、メインチェーンに置くと**全レイヤーでカーソルが停止**します。

対策として `config/gestures.dtsi` では `layer_listeners` による ON/OFF をやめ、各プロセッサに `always-active` を
付けています。プロセッサはいずれも `route_lN` のチェーンにしか存在せず、そのレイヤーでしか入力を受け取らないため、
レイヤー連動の体感は変わりません。これにより `g_suppress_cursor` は `&mg_set` を押したときだけ立つようになります。

### ノード名 `mouse_gesture` は使えない

`mouse-gesture.dtsi` が同名のビヘイビアノード（`compatible = "zmk,behavior-mouse-gesture"`）を定義しており、
DTS 上でマージされて binding 不一致でビルドが落ちます。本構成では `rpc_mouse_gesture` にしています。

### RPC が書き換えるのは DT インスタンス0番だけ

`zmk_mouse_gesture_runtime_set()` は `DEVICE_DT_GET(DT_DRV_INST(0))` 固定です。
RPC 用ノードを `torabo_tsuki_lp_right.overlay` に置いて0番を取らせることで、
`gestures.dtsi` の6セット（1〜6番）は DT 定義のまま維持されます。**ノードの定義順を変えると壊れます。**

### `ZMK_RUNTIME_MACRO_MAX_BYTES` は単独では設定できない

許容範囲が `range ZMK_CUSTOM_SETTINGS_VALUE_MAX_SIZE ZMK_CUSTOM_SETTINGS_LARGE_VALUE_MAX_SIZE` で決まります。
`LARGE` 側の既定は `VALUE` 側と同じ 64 なので、そのままだと範囲が `[64, 64]` に潰れて 256 が弾かれます
（ZMK は Kconfig 警告をエラー扱いします）。`CONFIG_ZMK_CUSTOM_SETTINGS_LARGE_VALUE_MAX_SIZE=256` が必要です。

### `zmk-module-battery-history` は機能を使わなくても必要

`torabo_tsuki_lp.dtsi` が `<behaviors/battery_history_request.dtsi>` を include しているため、
west.yml から外すと devicetree の前処理で落ちます。CONFIG は無効のままで構いません。

### `CONFIG_ZMK_BOARD_COMPAT` を自前で補っている

ZMK のビルドワークフローには、ボードがこのシンボルを宣言していない場合に
`west boards ... | grep "zmk"` を実行するチェックがあります。`bmp_boost` に zmk バリアントは無いため grep が空振りし、
当該ステップが `sh -e` で実行されているせいで**警告どまりの経路に到達せず必ず失敗**します。

`zmk-component-bmp-boost` の `Kconfig.bmp_boost` が `select ZMK_BOARD_COMPAT` を宣言するまでの回避として、
リポジトリ直下の `Kconfig` から `BOARD_BMP_BOOST` 限定で既定値を与えています。

### `&mg_set N` は Studio から割り当てられない

Studio はカスタム behavior のパラメータを保持できないため、`#binding-cells = <1>` の `&mg_set N` は
Studio のキー割り当て一覧から使えません。

モジュールは回避策としてパラメータ無しの `compatible = "zmk,behavior-mg-set-fixed"` とドライバを持っていますが、
**肝心のノードを定義しておらず、README にも記載がありません**。利用側で自分で書く必要があります。
`config/keymap.keymap` で `mg_set_0` / `mg_set_1` / `mg_set_2` を定義しているのがそれです。

セット番号は `binding_set_id()` が**ノード名の末尾1文字**から決めるので、ノード名は必ず `_0` / `_1` / `_2` で
終わらせてください。`display-name` が Studio の一覧に出る名前になります。

### RPC モジュール内蔵の慣性スクロールはトラックボールに向かない

`zmk-module-mouse-gesture-rpc` は自前の慣性スクロール
（`zmk,input-processor-inertial-scroll`）を持っており、**DYA Studio から実機調整できる**という
利点があります。参考にした moNa2 の構成もこちらを使っています。一度乗り換えを試しましたが、
**トラックボールでは使い物にならなかった**ので `mjmjm0101/zmk-input-processor-scroll-inertia` に
戻しました。試行はブランチ `v0.4+dya-studio+gesture+rpc-inertia` に残してあります。

**理由1: 初速を「最後のイベント1個」から取っている**

```c
/* input_processor_inertial_scroll.c */
data->velocity[idx] = (event->value * impulse_percent * Q_ONE) / 100;
data->ticks = 0;
k_work_reschedule(&data->work, K_MSEC(idle_ms));   // イベントごとにタイマーを再設定
```

スクロールイベントが来るたびに速度を上書きし、アイドルタイマーもリセットします。指を離した瞬間が
最高速であるトラックパッド／タッチ向けの設計です。

トラックボールは弾いた後もボールが物理的に回り続け、センサーは減速していく過程を報告し続けます。
`idle_ms` の空白ができるのは**ボールがほぼ止まった瞬間**なので、初速として拾われるのは
ピークではなく止まりかけの値になります。「ガーンと弾いても進まない」という症状になります。

**理由2: 弾き検出が無い**

小さい動きと大きい弾きを区別する仕組みがありません。初速は `value × IMPULSE%` の線形で、
しかも上記のとおり `value` はどちらの場合も 1 付近に潰れます。スクロール倍率をどう変えても
埋まりません。

**理由3: 速度が1単位を切っても出力が減らない**

```c
int16_t out = (int16_t)CLAMP(v / Q_ONE, INT16_MIN, INT16_MAX);
if (out == 0) {
    out = v > 0 ? 1 : -1;      // 0 になったら強制的に ±1 を送る
}
```

減速せず同じ速さで転がり続けます。`MIN VELOCITY` を 256 未満にすると、この分岐に入って
ダラダラ動き続けます（256 = 1スクロール単位）。RPC 版を使う場合は
**`MIN VELOCITY >= 256` 必須**です。

**mjmjm0101 版はこれらを全部解決しています。** 1,174行の実装で、IDLE / TRACKING / COASTING の
状態機械、速度の EMA、ピーク追跡、軸反転処理を持ち、`start` / `move` / `min-events` で
「本当に弾いたのか」を判定します。DTS 固定で再ビルドが必要なのが唯一の欠点ですが、
挙動が段違いです。

### スクロールの調整はどこで効くか

倍率が掛かる箇所が複数あるので整理しておきます。

```
ボールの回転
 ├─ ① センサー (PAW3222) の CPI        res-cpi。未指定＝ドライバ既定値。要再ビルド
 ├─ ② zip_xy_transform                 反転のみ
 ├─ ③ &mg (ジェスチャー)               通過のみ
 ├─ ④ mouse_runtime_input_processor    Studio の "mouse"。全レイヤーで有効
 ├─ ⑤ scroll_runtime_input_processor   Studio の "scroll"。DT 既定 1/60
 ├─ ⑥ zip_scroll_snap                  軸固定のみ
 ├─ ⑦ scroll_inertia_free              慣性（下記パラメータ）
 └─ ⑧ OS 側                            Windows「1回に3行」/ iPadOS のスクロール速度
```

**④が全レイヤーで有効なため、実効倍率は「mouse の倍率 × scroll の倍率」になります。**
ポインタ速度を変えるとスクロール速度も動く点に注意。

慣性側のパラメータは以下。`start` / `move` / `min-events` が「大きく弾いたときだけ効かせる」の
門番です。ここを緩めすぎると小さい動きでも慣性に入り、メリハリが消えます。

| | 既定 | 本構成 | 意味 |
|---|---|---|---|
| `start` | 40 | 40 | 慣性に入る最低ピーク速度 |
| `move` | 80 | 60 | 発動に必要な累積移動量 |
| `min-events` | 10 | 8 | EMA 収束待ち・ノイズ除去 |
| `friction` | 35 | 20 | 毎ティックの定数減速（千分率）。下げると伸びる |
| `limit` | 600 | 900 | 速度上限。上げると弾きが伸びる |
| `gain` / `blend` | 300 / 700 | 同左 | EMA の重み。合計 1000 |

**多段減衰**（速度域ごとに減衰率を変える）はこう入れてあります。

| | 値 | 意味 |
|---|---|---|
| `fast` | 250 | これを超えた速度＝「大きく弾いた」 |
| `decay-fast` | 998 | 高速域。減りにくい＝長く伸びる |
| `decay-slow` | 988 | 中速域 |
| `slow` | 60 | ここから下がテールゾーン |
| `decay-tail` | 985 | 止まり際。早めに畳んでダラダラさせない |
| `span` | 12000 | 慣性継続の安全上限（既定 6000）。`decay-fast` を緩めると自然減衰より先にここで切られる |

速度の目盛りは `start` (40) から `limit` (900) までなので、境界はその間に置きます。

#### ★`decay-*` は減衰率であって増幅率ではない

千分率/tick で、**常に 1000 未満**です。「速く弾いたら速度が倍になる」仕組みはモジュールに存在しません。

```c
/* src/scroll_inertia_math.h */
static inline int32_t select_decay_rate(int32_t speed, ...) {
    return speed > cfg->fast_fp ? cfg->decay_fast
         : speed > cfg->slow_fp ? cfg->decay_slow
         :                        cfg->decay_tail;
}
```

できるのは「速い間は減りにくくする」＝**伸びる**という形だけです。
初速を上げたいなら `limit`、全体倍率なら `scale` / `scale-div`。

#### ★モジュールの既定値は前提が違う

binding に「defaults are tuned for a **1000 CPI PMW3610 at 125 Hz**」とあります。
本構成は `tick = 17`（既定 8）なので **約 67Hz**。`decay-*` は tick ごとに掛かるため、
**実時間あたりの減衰が既定の感覚と違います。**
990 なら1秒で ×0.55 ですが、既定の tick=8 だと ×0.29 です。
ドキュメントの既定値をそのまま持ち込むと想定より伸びます。

#### 未使用のプロパティ

| プロパティ | 何ができるか |
|---|---|
| `swap-mod` / `unlock-mod` | **1つのスクロールレイヤーのまま修飾キーで軸を切り替える。** `swap-mod` 押下中は軸反転、`unlock-mod` 押下中は軸ロック解除（2D 自由） |
| `decel-samples` / `decel-ratio` / `peak-decay` | 弾き終わりの検出タイミング（TRACKING → COASTING の遷移条件） |
| `track-remainders` | 端数の持ち越し |

作者は README で**軸の自動判定をはっきり否定しています**（「45°付近、方向転換、ゆっくりドリフトは
バグではなく、入力データに存在しない情報を読もうとしているから起きる」）。
本構成は `axis = <0>`（軸ロック無し）＋ kot149 の `zip_scroll_snap`（＝まさに自動判定）で
思想としては逆を行っています。**斜めや切り返しで引っかかるならここが原因**で、
対処は上記の `swap-mod` / `unlock-mod` です。

### Studio で変えた値はリポジトリに残らない

`scroll_runtime_input_processor` の `scale-multiplier` / `scale-divisor`、
ランタイムコンボ / マクロ / ジェスチャーの内容、BLE のペアリング——これらは
DYA Studio から実行時に変更でき、**その値は Flash にだけ保存されます。**

* リポジトリを見ても現在値は分かりません
* **`settings_reset` を焼くと全部 DT / Kconfig の既定に戻ります**（スクロール倍率なら 1/60）
* ファームには config のバージョンも埋まらないので、後から突き合わせる手段もありません

再設定が必要になることを前提に運用してください。
気に入った値があるなら、DT 側の既定を実機に合わせて書き換えるのが確実です。

### レイヤーに `display-name` が無いと DYA Studio のパネルが空欄になる

ZMK のレイヤー名はこう決まります。

```c
// app/src/keymap.c
#define LAYER_NAME(node) DT_PROP_OR(node, display_name, DT_PROP_OR(node, label, ""))
```

どちらのプロパティも無いと**空文字**になります。Studio のキーマップタブは「レイヤーN」と番号で
代替してくれるので気づきませんが、**「OSごとのデフォルトレイヤー」パネルのプルダウンは
レイヤー名をそのまま表示する**ため、選択肢が全部空欄になって何も選べなくなります
（`zmk-feature-default-layer` 付属の参照 UI は `Layer ${i}` と番号でフォールバックするので、
これは DYA Studio 側の実装差です）。

全レイヤーに `display-name` を付けてあります。上限は `CONFIG_ZMK_KEYMAP_LAYER_NAME_MAX_LEN`
（既定 20 文字）です。

### 常時 ON のレイヤーを末尾に置いてはいけない

ZMK はキー押下時に**インデックスの高いレイヤーから順に**探します。

```c
// app/src/keymap.c  zmk_keymap_position_state_changed()
for (int layer_idx = ZMK_KEYMAP_LAYERS_LEN - 1;
     layer_idx >= LAYER_ID_TO_INDEX(_zmk_keymap_layer_default); layer_idx--) {
```

つまり `default-layer` で切り替えるベースレイヤーを末尾に置くと、常時 ON のあいだ
**全てのモーメンタリなレイヤーを上書き**します。実際、iPad レイヤーを index 12 に置いた版では
`MOUSE` レイヤーの `&to 0` が潰れ、**iPad モードだと MOUSE レイヤーから抜けられなくなっていました**。
`&bt BT_CLR` などの Bluetooth 操作も同様に効かなくなります。

ベースレイヤーはレイヤー0の直上（index 1）に置き、ファンクションレイヤーはその上に並べます。
既存のレイヤー番号をずらすことになるので、`&lt` / `&mo` / `&to` に加えて
`config/gestures.dtsi` の `GESTURE_ROUTE()`、`torabo_tsuki_lp_right.overlay` の
`active-layers = <BIT(N)>` と慣性スクロールの `layer`、
`CONFIG_ZMK_DEFAULT_LAYER_MIN/MAX_INDEX` を**すべて**追従させる必要があります。

なおレイヤー数の上限は **32枚**です。

```c
typedef uint32_t zmk_keymap_layers_state_t;   // 1レイヤー = 1ビット
```

`BUILD_ASSERT` で守られていないため、超えてもビルドは通り実行時に静かに壊れます。

### `ZMK_OS_DETECTION_USB` は USB の列挙経路を変える（DYA Studio 不調の原因ではなかった）

**経緯を含めて残します。同じ推論を繰り返さないためです。**

このシンボルは `CONFIG_USB_DEVICE_BOS` を `select` し、USB デバイス記述子の
`bcdUSB` を 2.00 から 2.01 へ上げます。すると host が `GET_DESCRIPTOR(BOS)` を
読みに来るようになり、USB の列挙経路そのものが変わります。

モジュール側にも `docs/windows-usb-enumeration-issue.md` として
**「Windows がデバイスを認識しなくなった（macOS/Linux は無事）」**という記録があり、
原因（Zephyr の BOS 記述子が `wTotalLength=0` の不正な状態で返る）を突き止めて
BOS capability を登録する修正が `main` に入っています。ここで使っているのは
その修正を含む `main`（`3052679`, 2026-07-19）です。

この記録があったため「DYA Studio へ USB 接続できないのはこれが原因」と考えましたが、
**切り分けの結果、原因ではありませんでした。**

| 試したこと | 結果 |
|---|---|
| `=y` のファーム | Studio に USB 接続できない |
| `=n` に戻したファーム | やはり最初は繋がらない。**何度か接続し直すと繋がる** |

`=n` でも同じ症状が出る以上、このシンボルは無関係です。

そして症状の性質も違っていました。**列挙そのものが失敗しているのではなく、
接続が断続的に失敗するだけ**です（再試行すれば繋がる）。BOS 記述子の問題なら
デバイスが認識されないか常に失敗するはずで、再試行で繋がることはありません。

**現在は `=y` に戻してあります。** 一時期 `=n` にしていたのは「原因だから」ではなく、
USB 周りを切り分けている間に変数を1つ減らすためでした。その切り分けを打ち切ったので理由が消えました。

副作用そのものは実在します。**USB でキーボードが認識されなくなったら、真っ先にここを疑ってください。**

**この件で2回同じ失敗をしています。**「モジュールのドキュメントに同じ症状の記録がある」
という状況証拠だけで原因と断定し、README に書きました。1度目は
「ポートの奪い合いが同時期に起きていた」ため確定できず、2度目は `=y` で再現したのを
確認として扱いましたが、`=n` でも再現したので確認になっていませんでした。

教訓は2つです。

* 切り分けは「有効にして再現する」ではなく**「無効にして消える」**を見ること
* **断続的な症状を1回の観測で判定しないこと。** 再試行で繋がる症状だったので、
  1回失敗しただけでは何の証拠にもなっていなかった

**繋がらないときの手順**（断続的なので、順に試すこと）。

1. **他のタブ／アプリがポートを掴んでいないか**確認する。Mouse Gesture Studio と
   DYA Studio を同時に開いているとこの症状が独立に起きます（後述の Import 失敗の項を参照）。
   こちらは実際に確認済みの原因です
2. Disconnect → Connect をやり直す。**何度か繰り返すと繋がります**
3. それでも駄目ならブラウザのタブごと閉じて開き直す

### `ZMK_DEFAULT_LAYER_MIN/MAX_INDEX` はパネルの選択肢ではない

名前から「デフォルトレイヤーとして選べる範囲」に見えますが、実際に使われているのは
`src/default_layer.c` の `apply_layer()` にある**強制 deactivate のループ範囲**です。

```c
for (int i = MIN_INDEX; i <= MAX_INDEX; i++) {
    if (i != layer && i != global_default) {
        zmk_keymap_layer_deactivate(...);
    }
}
```

パネルの選択肢は RPC が返す `layer_count`（= 全レイヤー数）から作られるので、この範囲を絞っても
レイヤー0は選べます。逆に範囲を広く取ると、モーメンタリなレイヤーが切替の巻き添えで落とされ得ます。

本構成はベースレイヤーが 0 と 1 の2枚なので、`MIN=MAX=1` としています。
レイヤー0は `global_default` としてループから除外されており、そもそも落ちません。
`MAX` を 12 まで広げると、モーメンタリなレイヤー 2..12 が切替の巻き添えで落とされます。

### コンボが keymap と DYA Studio の両方にあるとき、勝つ側はリンク順で決まる

`CONFIG_ZMK_RUNTIME_COMBO=y` なので、**コンボエンジンが2つ動いています。**

* ZMK 本体の DT コンボ（`config/keymap.keymap` の `zmk,combos`）
* `cormoran/zmk-feature-runtime-combo`（DYA Studio の「マクロ&コンボ」タブ）

両方が独立に `zmk_position_state_changed` を購読しており、**先に `CAPTURED` を返した方で打ち切られます。**

```c
/* app/src/event_manager.c:36 */
case ZMK_EV_EVENT_CAPTURED:
    return 0;          // 後続のリスナーには一切渡らない
```

ランタイム側は、その位置に有効なコンボが1つでもあれば必ず CAPTURE します。

```c
/* zmk-feature-runtime-combo/src/runtime_combo/runtime_combo.c:1119 */
int candidates = count_candidates_for_position(data->position, data->timestamp);
if (candidates == 0) { ... return ZMK_EV_EVENT_BUBBLE; }
...
return ZMK_EV_EVENT_CAPTURED;
```

**同じキー位置に両方置くと、片方は一度も発火しません。**
しかもどちらが先に走るかは**優先度ではなくリンク順**です（`.event_subscription` セクションを前から舐めるだけ）。
設定からは決まらず、ビルドによって入れ替わりうるので、症状は「たまに効かない」ではなく「そのビルドでは常に効かない」になります。

片方にしか無ければ、相手は `BUBBLE` して素通しするので問題ありません。

**両方に置くこと自体は害ではありません。** 問題になるのは中身が食い違うときだけです。

| 両方に置いたとき | 結果 |
|---|---|
| 同じバインディング | **どちらが勝っても同じ結果。むしろリンク順に依存しなくなる** |
| 違うバインディング | どちらが勝つかリンク順任せ。症状が読めない |

同じ内容を両方に置いておくと、`settings_reset` でランタイム側が消えても
keymap 側が残るという保険にもなります。**ただし片方だけ直すと食い違う**ので、
値を変えるときは必ず両方揃えること。どちらのスロットに入れたかは
リポジトリから見えないので、控えておくこと。

**ランタイム側のエンジンが寛容なわけではありません。** 窓の測り方は本体とほぼ同一で、既定値も同じ 50ms です。

```c
/* runtime_combo.c:878 */
if (pending_keys[0].data.timestamp + timeout_ms <= timestamp) return false;
/* app/src/combo.c:237 */
if (pressed_keys[0].data.timestamp + combos[i].timeout_ms > timestamp)
```

「DYA 側に同じものを作ったら効くようになった」場合、**DT 側が効き始めたのではなく、
勝っている側（＝ランタイム側）の中身が直った**と考えるほうが自然です。
それは裏を返せば、**消したはずの古いスロットが生きていた**ということです。次項参照。

#### ★Studio の「削除」ではキー位置が消えません

```c
/* runtime_combo.c:760 */
int zmk_runtime_combo_delete(uint32_t index, bool persist) {
    ...
    combo.enabled = false;        // ← これだけ
    return zmk_runtime_combo_write(index, &combo, persist);
}
```

RPC の応答も `"Combo disabled"` です。スロットにはキー位置が入ったまま、`enabled` ビットが落ちるだけ。
無効なら読み飛ばされる（`read_enabled_combo` が `-ENOENT`）ので機能上は足りますが、**2つ罠があります。**

1. **`persist` は Web UI 任せ**（`handle_delete_combo` が `req->persist` をそのまま渡す）。
   `save` / `discard` が別 RPC なので、**削除後に Save を押していないと RAM 上だけの削除**になり、
   再起動で Flash から復活します。
2. **スロットを本当に空にするのは Reset だけ**（`zmk_runtime_combo_reset()` が空バイト列を書く）。

**対処: Delete ではなく Reset してから Save。そのあと電源を入れ直して消えたままか確認する。**

#### どちらに置くか

| | keymap（DT） | DYA（ランタイム） |
|---|---|---|
| 変更 | 要再ビルド・再書き込み | 焼き直し不要 |
| リポジトリ | **残る** | **残らない** |
| `settings_reset` | 生き残る | **消える** |

**keymap 側を正とします。** DYA 側だけに置くと、`settings_reset` で消えたときに
どこにも記録が残りません（スクロール倍率と同じ問題）。

現状 `combo_ctrl_alt_tab` は**両方に同じ内容で入っています**（実機で効きが改善したのがこの形）。
片方だけ直さないこと。

なお `CONFIG_ZMK_RUNTIME_COMBO_DEFAULT_TIMEOUT_MS=50` はランタイムコンボ専用のグローバル設定で、
DT コンボには掛かりません。DT 側の既定は binding の `timeout-ms` = 50 です（別物）。

### 同じコンボでも、DYA 側で持たせたほうが安定することがある（原因未解明）

`combo_ctrl_alt_tab`（位置 15/16 = `&lt 11 U` + `&lt 8 COMMA` → `&kp LA(LC(TAB))`）で
実機で観測した挙動です。

| 状態 | 結果 |
|---|---|
| keymap のみ | 効いたり効かなかったり |
| keymap + **DYA 側にも同じ内容** | **毎回発動する** |
| keymap + DYA 側を無効化 | 効いたり効かなかったり |

**DYA 側が有効なときだけ安定します。** ランタイム側のリスナーが先に走って
`CAPTURED` を返すため、この状態では本体の DT コンボにはイベントが届いていません。
つまり**ランタイム側のエンジンが処理しているときだけ安定している**ということです。

**なぜそうなるかはソースからは説明がつきませんでした。** 確認した範囲では両者は同条件です。

| | ZMK 本体（DT） | ランタイム（DYA） |
|---|---|---|
| `timeout-ms` 既定 | 50 | 50（ファーム・Web UI とも） |
| `require-prior-idle` 既定 | 実質無効（-1） | 実質無効（0 で明示的に無効） |
| `slow-release` 既定 | `false` | `false`（`runtime_combo.c:123`） |
| 2キーコンボの処理経路 | 候補の絞り込み・完成判定・キーアップ時の flush とも等価 | 同左 |

**検証して否定した仮説**

* **出力側（`&kp` の修飾キー）ではない。** DYA 側にもマクロではなく同じ
  `&kp` 相当を割り当てており、それで安定している
* **BLE のジッターではない。** 同じ左手の `LANG_2`（位置 27/28）が既定の 50ms で支障なく効いている
* **古いスロットの残骸ではない。** 無効化した状態でも症状が出る

**まだ潰せていない可能性**

* ~~DYA 側のコンボに 50 より大きい `Timeout ms` が入っている~~
  → **実機で確認済み。何も設定しておらず既定のまま**だった。この線は消えた
* 本体側エンジンの何かを読み落としている。詰めるなら USB コンソールで
  `combo.c` の `LOG_DBG`（候補の絞り込みとタイムアウト）を見るのが確実

**調査はここで打ち切りました。** 動いているので実害がありません。

**現状の扱い**: keymap と DYA の両方に同じ内容を置き、**DYA 側を有効のまま**にしています。
中身が同じである限り、どちらが勝っても結果は同じです（上記「勝つ側はリンク順で決まる」参照）。
keymap 側の `timeout-ms` は 80 にしてありますが、これは上表の差の説明ではなく、
15/16 が上段の2キー同時押しでホームポジションの `LANG_2` より指が揃いにくいことへの手当てです。

### 左手のコンボの窓は BLE の届き方を測っている（今回の原因ではなかった）


分割では、**central が peripheral のタイムスタンプを捨てて自分の時計で打ち直します。**

```c
/* app/src/split/central.c:46 */
struct zmk_position_state_changed state_ev = {
    .position = ev.data.key_position_event.position,
    .state    = ev.data.key_position_event.pressed,
    .timestamp = k_uptime_get()};      // ← peripheral の時刻は使わない
```

コンボ判定はこの時刻で窓を測ります（`combo.c:237`）。

```c
if (pressed_keys[0].data.timestamp + combos[i].timeout_ms > timestamp)
```

つまり **peripheral 側（いまなら左手）のコンボの `timeout-ms` は、
「指が同時に押せたか」ではなく「BLE が同時に届けたか」を測っています。**
接続間隔は 7.5〜15ms（`BT_PERIPHERAL_PREF_MIN_INT=6` / `MAX_INT=12`）で、再送が挟まると窓をかなり食います。
理屈の上では peripheral 側の窓は際どくなり得ます。

**ただし今回の Ctrl+Alt+Tab の原因ではありませんでした。**
同じ左手の `LANG_2`（位置 27/28）が既定の 50ms で支障なく効いているので、
この構成では 50ms で足りています。一度 80ms に広げましたが戻しました。
左手のコンボが軒並み不安定になったときに、ここを思い出してください。

広げるコストは小さいです。`position_state_up` が離した時点で `cleanup()` を呼んで
取り込んだイベントを流すので、**普通に打つぶんにはタイムアウトを待たされません。**
hold-tap の長押しでレイヤーに入るときだけ `timeout-ms` 分だけ遅れます。

### Studio のロックはキーマップしか守らない（マクロの中身は読める）

`CONFIG_ZMK_STUDIO_LOCKING=y` にしても、**DYA Studio のカスタム機能はほぼ素通りです。**
名前から期待する「ロック＝設定全部が触れない」ではありません。

**ロックが弾くのは `SECURED` と宣言されたハンドラだけです。**

```c
/* app/src/studio/rpc.c:46 */
if (sub_handler->security == ZMK_STUDIO_RPC_HANDLER_SECURED &&
    zmk_studio_core_get_lock_state() != ZMK_STUDIO_CORE_LOCK_STATE_UNLOCKED) {
    return ZMK_RPC_RESPONSE(meta, simple_error, zmk_meta_ErrorConditions_UNLOCK_REQUIRED);
}
```

ZMK 本体のキーマップ系は 12 個すべて `SECURED` です
（`app/src/studio/keymap_subsystem.c:527-538`）。**キーマップは守られます。**

いっぽう DYA Studio のカスタム RPC は**サブシステム単位**で security を持ち、
`main` の各モジュールを実際に見ると次のとおりです。

| サブシステム | security |
|---|---|
| `cormoran__pmw3610` | **SECURED**（`WriteRegister` を持つため） |
| `cormoran__kscan_diagnostics` | **SECURED**（`..._STUDIO_RPC_UNSECURED=y` で外せる） |
| `cormoran__runtime_macro` | UNSECURED（後述の自前ゲートあり） |
| `cormoran__runtime_combo` | UNSECURED |
| `cormoran_custom_settings` | UNSECURED |
| `cormoran__runtime_input_processor` | UNSECURED |
| `cormoran__default_layer` | UNSECURED |
| `cormoran__os_detection` | UNSECURED |
| `shakushakupanda` のジェスチャー RPC | UNSECURED |

`UNSECURED` を既定にするのは cormoran さんの設計方針です
（`zmk-feature-kscan-diagnostics/skills/zmk-module-design/SKILL.md`）。

#### マクロの中身は、ロック中でも読み出せる

ランタイムマクロだけは二重になっていて、**結局は読めます。**

まず runtime-macro モジュールは自前のゲートを持っていて、
`ListMacros`（スロット番号・名前・サイズだけ）と `GetMacroGlobalSettings` 以外は
ロック中に弾きます（`runtime_macro_handler.c:872-880, 899`。2026-07-18 の PR #18 で追加）。
ここだけ見ると守られているように見えます。

**しかしマクロ本体は custom-settings のキースペース `runtime_macros`
（キー `macro/<名前>`）にそのまま置かれていて、そちらの read 権限は `UNSECURE` です。**

```c
/* zmk-feature-runtime-macro/src/runtime_macro.c:112 */
ZMK_CUSTOM_SETTING_KEYSPACE_DEFINE_WITH_POOL_SIZE(
    runtime_macros, ZMK_RUNTIME_MACRO_SUBSYSTEM_ID, ZMK_RUNTIME_MACRO_KEY_PREFIX,
    ZMK_CUSTOM_SETTING_VALUE_TYPE_BYTES, ...,
    ZMK_CUSTOM_SETTING_CONFIDENTIALITY_RPC_PUBLIC,
    ZMK_CUSTOM_SETTING_PERMISSION_UNSECURE,   /* ← read  */
    ZMK_CUSTOM_SETTING_PERMISSION_SECURE,     /* ← write */
    ZMK_CUSTOM_SETTING_NO_CONSTRAINT);
```

キースペースの権限はスロットにそのまま継承され（`custom_settings_keyspace.c:154`）、
custom-settings の `GetSetting` は `read_permission` しか見ません
（`custom_settings_handler.c:1603`）。

つまり **`cormoran_custom_settings` 側から `macro/<名前>` を読めば、
ロック中でもマクロ本体のバイト列が返ります。** runtime-macro のゲートは迂回できます。

書き込みはキースペース・サブシステムとも `SECURE` なので、**ロック中に改変はできません。
読めるだけです。**

#### 結論

| | ロック中 |
|---|---|
| キーマップの読み書き | **不可** |
| マクロ・コンボ・ジェスチャー・各種設定の**書き込み** | **不可** |
| マクロの中身の**読み出し** | **可**（上記の経路） |
| コンボ / ジェスチャー / 入力プロセッサ設定 / OS 判定の**読み出し** | **可** |
| センサーのレジスタ操作・キー診断 | 不可（`SECURED`） |

**ランタイムマクロの中身は Studio ロック中でも読み出せます。**
キーマップに直書きするよりは良い（リポジトリの履歴には残らない）というだけです。

守るなら Studio の BLE トランスポートを切って物理アクセスを必須にします。

```conf
CONFIG_ZMK_STUDIO_TRANSPORT_BLE=n   # 既定は y
```

なお BLE のままでも GATT キャラクタリスティックは
`BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT` なので、
**ペアリング済みのホストからしか触れません。**
「誰でも近づけば読める」わけではなく、自分のPC・スマホが前提です。

---

## ビルド構成

**作るのは3つだけです。** 組み合わせを全部ビルドすると CI 時間を食うので、
実際に使う構成だけに絞っています。

| 成果物 | 中身 |
|---|---|
| `torabo_tsuki_lp_right_central` | 右手 = トラックボール + Studio + キーマップ |
| `torabo_tsuki_lp_left_peripheral` | 左手 = キーのみ |
| `settings_reset` | 設定消去 |

**左右は必ず組で焼いてください。** central 側が Studio・HID・キーマップを持ちます。

### ★ミニトラックパッドが届いたら

**左右そろえて差し替えます。** 左だけ変えても動きません。

```yaml
left:  "studio-rpc-usb-uart input-trackpad-mini input-split"
right: "studio-rpc-usb-uart split-central input-trackball input-listener
        input-split-listener input-scroll-inertia"
```

左のパッドは `scroller-mode` なので、指の移動は X/Y ではなく `INPUT_REL_WHEEL` として飛び、
`input-split` で右手 central へ転送されます。**受ける側が `input-split-listener`** です。

* カーソルとして使いたいなら左を `input-trackpad` に差し替える
* **向きが逆なら** `snippets/input-trackpad-mini/` の DT に `v-invert` / `h-invert` を足す

**届く前に焼かないこと。** `CONFIG_IQS7211E=y` と i2c の DT ノードだけが入った状態になり、
繋がっていないデバイスをプローブし続けます（`power-gpios` も駆動されます）。

### 他の構成に戻したいとき

**スニペットは全部リポジトリに残してあります。** `build.yaml` の `snippet` 行を差し替えるだけです。
書き方は `build.yaml` の冒頭コメントに並べてあります。

| やりたいこと | 左 | 右 |
|---|---|---|
| 両手にトラックボール | `... input-trackball input-split` | パッドのときと同じ |
| 左に4方向スイッチ ★左手側専用 | `... kscan-4-direction-switch` | `... kscan-4-direction-switch-central` |
| 左右を入れ替える | `... split-central input-trackball input-listener` | `studio-rpc-usb-uart` |
| Extender Mini で右手に2個目 ★未検証 | — | `... input-ext-trackpad` を足す |

### 拡張モジュールは片側に1つだけ

トラックボール・トラックパッド・ハイレゾダイヤル・4方向スイッチの**4つが同じ拡張 FFC ポートを取り合います。**

| | トラックボール | トラックパッド | ダイヤル | 4方向スイッチ |
|---|---|---|---|---|
| バス | `spi0` | `i2c0` | `i2c0` / `0x75` | GPIO 直 |
| ピン | SCK P0.18 / MOSI・MISO P0.16 | SDA P0.18 / SCL P0.16 | 同左 | P0.16 / P0.18 |
| 電源 | P0.8 | P0.8 | P0.8 | — |
| 割り込み | P0.19 (irq) + P0.20 (CS) | P0.20 (irq) | P0.20 (motion) | P0.19 / P0.20 |

`spi0` と `i2c0` が**同じ P0.18 / P0.16 に載っている**のが効いています。
さらにトラックボールとトラックパッドのスニペットは `pointing_device` という同じノードラベルを使うため、DTS 上でも同居できません。

2個載せたいなら **左右で分担する**（構成 C〜E）か、
[`ngsyst/bmp_boost_extender_mini`](https://github.com/ngsyst/bmp_boost_extender_mini) で BMP Boost の追加IOポート（P0.17 / P0.21、割り込み P0.31、電源 P0.24）を引き出します。
後者の足場として `snippets/input-ext-trackpad/` を置いてありますが、**現物での検証はしていません**。build.yaml にも入れていません。

### トラックパッドの2つのスニペットは用途が違う

| snippet | 動作 |
|---|---|
| `input-trackpad` | **カーソル** — `INPUT_REL_X` / `INPUT_REL_Y` を出す |
| `input-trackpad-mini` | **スクロール** — `scroller-mode` で `INPUT_REL_WHEEL` を出す。ドライバ内蔵の慣性つき |

純正の「ミニトラックパッドオプション」は高分解能スクロール用なので後者が正解です。
v0.4 移行のときにこの `scroller-mode;` と `CONFIG_IQS7211E_SCROLLER_INERTIA=y` を落としてしまい、
2つのスニペットが同一になっていました（復旧済み）。

### 4方向スイッチは左手側専用

`snippets/kscan-4-direction-switch/` は右手側でビルドすると `#error` で止まります。
オフセットが二重に掛かるためです。

* kscan-composite が子 kscan の報告に `column_offset` を足す（`kscan_composite.c:113`）
* そのあと matrix-transform が自分の `col-offset` を足す（`matrix_transform.c:78`）
* 右手側の変換は `col-offset = <7>` を持つので、composite 側でも足すと 14..20 で範囲外に落ちる

右手側に付けたくなったら、変換の `map` を `RC(5,0..4)` から `RC(5,7..11)` へ動かしてください。
**上流の `feat/input-hires-dial` ブランチは同じ穴に落ちています**（コミットメッセージに「動作未確認」とあるのと整合します）。

分割では左右がそれぞれ自分の変換を適用してから position を送る（`app/src/physical_layouts.c`）ので、
**両半分が同じ形の変換を持っていないと位置がずれます。** そのため反対側の central にも
`kscan-4-direction-switch-central` でフラグだけ立てます。

### 慣性スクロールは central 専用

モジュール側に

```c
#if IS_ENABLED(CONFIG_ZMK_SPLIT) && !IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
#error "central role required"
```

というガードがあり、peripheral に DT ノードが来るとビルドが落ちます。
`torabo_tsuki_lp_right.overlay` は central / peripheral 両方で読まれるため、
ノードを `status = "disabled"` にしておき、`input-scroll-inertia` snippet で `okay` に上書きしています。

---

## マクロについて

キーマップに直接書いた内容は、リポジトリの履歴に残り続けます。
本構成では DYA Studio の**ランタイムマクロ**を使い、内容を Flash に保存しています。

1. Studio の「マクロ&コンボ」タブで名前を付けて **Create**（スロット番号が自動で割り当てられます）
2. ステップを編集して **Save**
3. 一覧で確認したスロット番号を `&rmacro <slot>` としてキーに割り当て

スロット番号は事前に選ぶものではなく、作成時に決まって再起動をまたいでも変わりません。
未作成のスロットを割り当てても何も再生されないだけです。

```conf
CONFIG_ZMK_RUNTIME_MACRO_COUNT=8          # マクロ数
CONFIG_ZMK_RUNTIME_MACRO_MAX_BYTES=256    # 1個あたり
CONFIG_ZMK_RUNTIME_MACRO_POOL_BYTES=1024  # 全マクロ合計
```

### ランタイムマクロが避けられるのは履歴だけ

避けられるのは **git の履歴に残ること**だけです。
**Flash 上の中身は Studio ロック中でも読み出せます**
（上記「Studio のロックはキーマップしか守らない」参照）。

加えて、DYA Studio / Mouse Gesture Studio の **Export した JSON には内容が平文で入ります。**
その JSON をリポジトリに入れたり、他人に渡したりしないこと。
