# phi_dm C++ 移植版（rel5g_MasterGate_plus）

Phantasmal Island（Philly）用ゲームサーバ `phi_dm`
（`rel5g_MasterGate_plus`、Tofup Server Edition v6.5g 'Master Gate +'）
の C++ への段階的書き換えです。

## PHIDM_NEO（公開版）について

このリポジトリは PHIDM_NEO のソース一式です。現在の公開版は
**v1.12** で、内容は次のとおりです。

- サーバ本体の全 16 モジュールを C++ 化し、潜在バグ 9 件を修正
- 11x11 マップ対応（`#ex-map size=121`、NeoPHI 等の対応クライアント向け。
  9x9（size=81）は v1.10 で仕様削除し、要求には従来地図で応答する）
- Eagle Eye を 25x25 に拡大
- マスター魔法の追加（Aegis of Athena、Remote lock/unlock、
  Remote boards、Remote erase、Thunder of Zeus、Hammer of Ares、
  Master search、Load class）
- 刻印スイッチ（動作 60/61、phi.hall）と Lua スイッチ
  （動作 62、phi.cls、Lua 5.4.7 同梱）
- FX 演出拡張（動作 63〜68 と :A チェーンの z ペイロード。
  マップ文字・暗転・ワイプ・画像・振動・発光）
- 2x2 の大きな敵に対応（敵データ `phi.ed` の `:O` オプションに
  `B` を書いた敵が 2x2 の体を持つ。4 マスの当たり判定・移動・
  正面 2 マス攻撃、地図では専用の大きさ印 `B` で 1 回だけ送る）
- 掲示板の検索結果など大量の行を送る際に、送信バッファの
  あふれで接続が切れる問題を対策（バッファ拡大と送出待ち）

すぐ動かしたい場合は、Releases から `PHIDM_NEO.zip` を
ダウンロードしてください。ビルド済みの `phi_dm` と
`cdcnv`、雛形ワールド（ポート 20017、サンプルアカウント
guest / master 入り）が入っています。雛形ワールドを
公開サーバに使う場合は、マスターアカウントのパスワード
変更や不要なアカウントの削除を忘れずに行ってください。

著作権と配布については同梱の `Notice` を参照してください。

**現状：全モジュール完了（第13段階まで完了）。** `tcp.cpp`・`phi_str.cpp`・`ext_party.cpp`・
`ext_data.cpp`・`new_command.cpp`・`new_proto.cpp`・`phi_m57.cpp`・
`expression_op.cpp`・`phi_calc.cpp`・`phi_use.cpp`・`ext_skills.cpp`・
`phi_attack.cpp`・`phi_enemy.cpp`・`phi_magic.cpp`・`phi_dm.cpp`・`phi_data.cpp` を C++ らしく書き換え済み
（振る舞いは変更なし、テスト用ワールドで前後比較済み）。サーバ本体の
モジュールはすべて書き換えが終わりました。修正内容の詳細は `docs/cxx-port-notes.md`
を参照してください。

ソースの文字コードは UTF-8（行末は CRLF）です。ビルド時は
`-finput-charset=UTF-8 -fexec-charset=CP932` を付けてコンパイルし、
送受信するバイト列を Shift_JIS（CP932）のままに保っています。
2026-11-15 に第7段階完了後、Shift_JIS から UTF-8 へ変換しました。

## ビルド

```sh
make            # phi_dm を作ります（g++ -std=c++17 -DBSD -O2 -Inames -finput-charset=UTF-8 -fexec-charset=CP932）
make cdcnv      # キャラクタデータ変換ツール cdcnv を作ります
make clean      # 生成物を消します
```

必要条件：g++（C++17 対応）、GNU make。外部ライブラリは不要です
（`-lm` のみ）。

新規に設置する場合は `./configure.sh` を実行すると、質問への
回答から `phi_dm.cnf` と Makefile が生成されます（詳細は後述の
「ビルドの標準手順」を参照）。上の `make` は、同梱の Makefile を
直接使う場合のものです。

### FreeBSD 8.1（gcc46）でのビルド

FreeBSD 8.1 の標準 g++ は C++17 に対応していないため、gcc46 を
使ってビルドします。/var が小さい環境ではコンパイラの一時
ファイルで /var が溢れるので、TMPDIR をホーム配下へ向けて
ください（csh の場合）。

```sh
mkdir -p ~/tmp
make clean
setenv TMPDIR ~/tmp
make CXX=g++46 CC=gcc46 CXXSTD=-std=gnu++0x LIBS="-lm -Wl,-rpath,/usr/local/lib/gcc46"
```

sh / bash では `TMPDIR=~/tmp make ...` の形でも構いません。

注意点：

- Makefile はヘッダ依存を追わないので、ソースや `phi_dm.cnf` を
  変えたら必ず `make clean` から全ビルドしてください。
- 旧 C 版の `.c` ファイルが同じフォルダに混ざっていると、BSD make
  が `.cpp` より `.c` を優先してエラーになります。あれば別の場所へ
  退避してください。
- できあがった `phi_dm` は gcc46 用 libstdc++ への rpath 付きで
  リンクされるので、そのまま実行できます。

### ビルドの標準手順（configure.sh）

ビルドの標準手順は、元の C 版と同様に `configure.sh` の実行です。
システム選択とコンパイラ（gcc / g++）の検出のあと設置先を聞かれ、
設置先に世界データが無い場合は世界設定の質問（エリア名、
マップ幅・高さ 8〜1000、NPC 数など）に答えると `phi_dm.cnf` が
生成され、最後に C++17 用の Makefile が生成されます。続いて
`make`、`make install` で実行ファイルが作成され、設置先へ
コピーされます。不足する世界ファイル（`phi.td` / `phi.swt`
など）の初期データも自動で生成されます。設置先に既に
`phi_dm.cnf` がある場合は、その内容がそのまま使われ、世界設定の
質問は出ません。

このツリーに同梱の Makefile を直接使うこともできます。その
場合、設定は既存の `phi_dm.cnf` がそのまま使われます。
`phi_dm.cnf` の `MAX_X` / `MAX_Y` は世界の地図サイズ
（マス数）で、世界の `phi.dd` の大きさに合わせてください
（同梱の 8 は最小の雛形用です。256×256 の世界なら 256）。
書き換えたら `make clean` から再ビルドが必要です。

注意：同梱 Makefile の `make install` は無効化してあります
（`configure.sh` が生成する Makefile の `make install` は
使えます）。同梱 Makefile でビルドした場合は、稼働中の
ワールドへ誤ってコピーしないよう、バイナリは手でコピーして
ください。

## テスト方法

稼働中のサーバには絶対に触れず、コピーしたテスト用ワールドで
確認します。

```sh
# 1. ワールドデータをコピーする（元データは触らない）
rsync -a --exclude='phi_dm' --exclude='server.out' --exclude='phi.log' \
  ~/workspace/phi-server/world/ ./test-world/

# 2. コピー側の phi.td の :O 行のポートだけを 20019 に変える
sed -i 's/^:O O 20017/:O O 20019/' test-world/phi.td

# 3. ビルドしたバイナリを置いて起動する
cp phi_dm test-world/
cd test-world && ./phi_dm
```

`phi.crt` / `phi.ex` / `phi.cmd` が無いという警告は無害です。

クライアント（NeoPHI）から `guest` でログインして確認します。
`~/workspace/phi-client-py/` にある Python クライアントを使います。

```sh
python3 -m neophi.cli --host 127.0.0.1 --port 20019 --id guest
```

確認が終わったら、テスト用サーバは PID を指定して停止します
（`pkill -f phi_dm` は自分のシェルまで殺すので禁止）。
`ss -tlnp` で稼働中のサーバ（20017/20018）が無事なことも確認します。

## ソース構成

| ファイル | 役割 | 行数 |
|---|---|---|
| `phi_dm.cpp` | メインループ・コマンド振り分け | 約5100 |
| `phi_data.cpp` | ワールドデータの読み書き | 約9900 |
| `phi_magic.cpp` | 魔法 | 約5800 |
| `phi_enemy.cpp` | 敵 NPC の思考・行動 | 約2700 |
| `phi_attack.cpp` | 戦闘 | 約1400 |
| `expression_op.cpp` | スクリプト式評価（swt） | 約1000 |
| `phi_calc.cpp` | 各種計算（ダメージ・命中など） | 約900 |
| `ext_skills.cpp` | スキル拡張 | 約700 |
| `phi_m57.cpp` | 57x57 地図転送 | 約650 |
| `phi_use.cpp` | アイテム使用 | 約620 |
| `tcp.cpp` | ソケット層 | 約530 |
| `phi_str.cpp` | 文字列整形（メッセージ組み立て） | 約310 |
| `new_proto.cpp` | 拡張プロトコル | 約250 |
| `ext_data.cpp` | 拡張データ | 約140 |
| `ext_party.cpp` | パーティ拡張 | 約100 |
| `new_command.cpp` | 追加コマンド | 約90 |
| `cdcnv.cpp` | キャラクタデータ変換ツール | 約300 |
| `bsd/` | `strlcpy` / `strlcat` 互換関数 | — |
| `names/` | 名称テーブル（アイテム・魔法など） | — |
| `doc/`・`man/` | 元のドキュメント類 | — |

`phi_dm.h` が全グローバル状態（キャラクタ配列 `cd[]`、地図 `map` など）
を持ち、全モジュールが include しています。

## 段階の進み具合

依存関係の浅い・小さいモジュールから、1つずつ C++ らしく
書き換えてきました（クラス化、`std::string` / `std::vector` 化、
コメントとドキュメントの充実）。実施した順番：

1. `tcp.cpp`（依存が最も浅い）→ `phi_str.cpp`（小さい）【完了】
2. `ext_party.cpp` / `ext_data.cpp` / `new_command.cpp`（小さい拡張群）【完了】
3. `new_proto.cpp` → `phi_m57.cpp` → `expression_op.cpp` → `phi_calc.cpp`【完了】
4. `phi_use.cpp` → `ext_skills.cpp` → `phi_attack.cpp` → `phi_enemy.cpp`【完了】
5. `phi_magic.cpp` → `phi_dm.cpp` → `phi_data.cpp`（中核3つは最後）【完了】

各モジュールの書き換えのたびに、テスト用ワールドで動作確認をしました。
