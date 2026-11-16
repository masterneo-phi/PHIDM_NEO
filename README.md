# phi_dm C++ 移植版（rel5g_MasterGate_plus）

Phantasmal Island（Philly）用ゲームサーバ `phi_dm`
（`rel5g_MasterGate_plus`、Tofup Server Edition v6.5g 'Master Gate +'）
の C++ への段階的書き換えです。

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

元の C ビルドは `configure.sh` + `make MAKEOPT="-fcommon"` でしたが、
このツリーでは `configure.sh` は使いません。設定は `phi_dm.cnf`
（`MAX_X` は 8 のこと。16 にするとエリアデータが壊れます）が
そのまま使われます。

注意：`make install` は無効化してあります。稼働中のワールドへ
誤ってコピーしないよう、バイナリは手でコピーしてください。

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
