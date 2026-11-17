# C++ 移植メモ（第0段階）

元ソース `rel5g_MasterGate_plus`（C言語）を、C++17 としてビルドできるようにした
第0段階の記録です。この段階では**振る舞いを変えないこと**だけを目標とし、
クラス化・`std::string` 化などのリファクタリングは一切していません。
ワールドデータ形式と通信プロトコルも変更していません。

## ビルド方法

```sh
make            # g++ -std=c++17 -DBSD -O2 -Inames -Wall
make cdcnv      # 変換ツールもビルドできます
```

元の C ビルドは `gcc -DBSD -O3 -fcommon` でした。`-fcommon` は不要になり、
代わりにヘッダーの暫定定義（tentative definition）を `extern` 化しています
（後述）。元の Makefile の `install` ターゲットは、稼働中のワールドへ
バイナリをコピーしてしまう危険があるため無効化してあります。

## 行った修正の種類

### 1. K&R 形式の関数定義をプロトタイプ形式に（53か所）

```c
/* 変更前 */
int new_user(n)
{
/* 変更後 */
int new_user(int n)
{
```

引数の型は、K&R 宣言行（`int n;` / `char *com;` / `ItemData *id;` など）
から取りました。宣言行のない引数は K&R の暗黙の `int` とみなしました
（ポインタ引数に宣言行がないと 64bit では壊れるため、C版が動いている
以上そのような引数は存在しません）。宣言ブロック内にあったコメントは
関数ヘッダーの前に残してあります。変換はスクリプトで行い、diff を
全て目視確認しました。対象は `phi_dm` / `phi_enemy` / `phi_attack` /
`phi_use` / `phi_calc` / `phi_magic` / `bsd/strlcpy` / `bsd/strlcat`。

### 2. ヘッダーのグローバル変数を `extern` 化（約110行・111シンボル）

C版は `-fcommon` に頼り、ヘッダーに書かれた `int foo;` のような暫定定義を
全翻訳単位で共有していました。C++ では多重定義エラーになるため、

- ヘッダー側の宣言を `extern` に変更
- 実体を1か所の `.cpp` に集約（`phi_dm.h` → `phi_dm.cpp`、
  `phi_data.h` → `phi_data.cpp`、`phi_m57.h` → `phi_m57.cpp`、
  `expression_op.h` の `stpt` → `expression_op.cpp`）

しました。11シンボル（`yslide`、`ed_o`、`enmax`、`MLine` ほか魔法系8つ）は
既に `.cpp` 側に実体があったため、ヘッダーだけ `extern` 化しました。
`dobj` は `#if MAX_OBJECTS > 255` で型が変わるため、実体側も同じ条件で
分岐させています。

どのシンボルがどこに実体を持つかは、元のCビルドの `.o` に `nm` をかけて
確認しました（全て COMMON のみで、強い定義を持つものはありませんでした）。

### 3. 構造体タグ `id_t` の改名（1か所）

`phi_dm.h` の `typedef struct id_t { ... } ItemData;` は、POSIX の
`id_t` 型（`<sys/types.h>`）と衝突して C++ ではエラーになります。
タグを参照している箇所は他に無かったため、`item_data_t` に改名しました。
`ItemData` 型そのものは変わっていません。

### 4. 文字列リテラルの三項演算子へのキャスト（44か所）

```c
/* 変更前 */ input_analyze(n, (cond ? NTK_A : NTK_B));
/* 変更後 */ input_analyze(n, (char*)(cond ? NTK_A : NTK_B));
```

C では文字列リテラルは `char*` ですが、C++ では `const char*` のため、
2つのリテラルの三項演算子は `const char*` になり、`char*` 引数に
渡せません。呼び出し側でキャストしています。

注意: `input_analyze` は引数のバッファへ書き込むことがある関数
（`cd[n].parse != 0` のとき `snprintf` / `strlcpy` する）なので、
引数を `const char*` に変えるのは誤りです。NPC からの呼び出しでは
`parse` は 0 のため、従来通りリテラルを渡しても実際には書き込まれ
ません（C版と同じ前提です）。将来の const 化ではこの点を踏まえて
ください。

### 5. `malloc` の戻り値へのキャスト（1か所）

`phi_data.cpp` で `CHRExData*` へ代入する `malloc` にキャストを追加
しました。

### 6. `socklen_t` への型変更（2か所）

`tcp.cpp` で `getsockname` / `accept` に渡す長さ変数を `int` から
`socklen_t` に変更しました（いずれもその呼び出し専用のローカル変数です）。

### 7. 値なし `return` の修正（2か所）

`int` を返す `next_level_check` / `level_up_print` の末尾の `return;` を
`return 0;` にしました。呼び出し側は戻り値を使っていません。

### 8. Makefile のヘッダー依存の追加

元の Makefile にはヘッダー依存が無く、ヘッダーを編集しても古い `.o` が
残ってリンクエラーの原因になりました。全オブジェクトが全ヘッダーに
依存するよう追加しました。

## 残っている警告について

`-Wall` で大量の警告が出ます（未使用変数、`-Wwrite-strings`、
符号比較など）。第0段階では振る舞いを変えないことを優先し、警告の
修正はしていません。モジュールごとの書き換えの際に、そのモジュールの
警告から片付けていくのがよいでしょう。

## 文字コードの注意

ソース内の日本語は UTF-8 で、行末は CRLF です。2026-11-15 に
第7段階の完了後、Shift_JIS から UTF-8 へ一括変換しました。
ビルド時は `-finput-charset=UTF-8 -fexec-charset=CP932` を付け、
文字列リテラルが送受信するバイト列を CP932 のままに保っています。
変換前後で文字列テーブルとセッション出力を比較し、一致を確認済みです。
編集時はエンコーディング（UTF-8）と改行（CRLF）を壊さないよう
注意してください。

## 動作確認（2026-11-15）

`test-world/`（`~/workspace/phi-server/world/` のコピー、ポートのみ
20019 に変更）で C++ 版 `phi_dm` を起動し、Python クライアント
（NeoPHI）から `guest` で確認しました。

- ログイン直後のメッセージ・7x7地図・状態表示がC版と同一
- `look`、`cast` → 魔法リスト（Cure/Insane）→ Cure の詠唱・回復
  （MP 1000→992、`[52 hp]` の回復行、HP 909→961）
- 敵同士の戦闘ログ（`[N hp]` / `[N mp]`、殺害、アイテム拾い・使用）
- ゲートの `go` → 目的地リスト → `inside house` への移動 → 移動

確認後、テストサーバは停止し、稼働中のC版2台（20017/20018）が
無事なことを確認しています。

## 第1段階：tcp.cpp / phi_str.cpp の書き換え（2026-11-15）

第0段階で残した「モジュールごとの書き換え」の最初として、依存の浅い
2モジュールを C++ らしく書き換えました。公開 API（`tcp.h` / `phi_str.h`）
と `phi_dm.h` のグローバル構造は変更していません。検証は、決まった
手順のクライアントセッション（ログイン・look・check・spells・cast・
ゲート移動など）を書き換え前後のバイナリで流し、時刻・乱数由来の
ゆらぎを正規化して応答を比べる方法で行いました（2回ずつ実行し、
第0段階同士でも同一になることを確認済み）。

### tcp.cpp

- 接続スロットを無名名前空間の `Client` 構造体＋`std::array` にし、
  送受信バッファを `malloc`/`free` から `std::unique_ptr`（RAII）に
  変更。内部ヘルパーはすべてファイルローカル化し、バッファの詰め
  処理は `memmove` ベースにしました。
- タイムアウト処理を `gettimeofday` ベースの残り時間計算から
  `std::chrono::steady_clock` の期限ベースに変更（壁時計のずれに
  影響されなくなります。`select()` の呼び回数・意味は同じです）。
- `sockaddr_in` の値初期化、`bzero()` の廃止。
- 内部の癖は保存しています：`peek_protocol()` は行頭の `#` 行の
  うち、実際にはバッファ先頭にあるものだけを拾います（行を
  読み飛ばす際に1文字進みすぎる従来の挙動）。コメントに記録
  してあります。
- バッファ解放時に長さも 0 にして「バッファが無いとき長さは 0」
  の不変条件を保つようにしました。従来は長さが古いまま残って
  いましたが、参照される経路は無いため振る舞いは同じです。

### phi_str.cpp

- `dmprintf()` の 39分岐の if-else の鎖を、キーワード→処理関数の
  テーブル駆動に変更しました。テーブルの順序は元の鎖と完全一致
  （機械的に照合済み）で、接頭辞の重なり（`ip` と `i`、`maxhp` と
  `hp` など）も従来通りです。展開結果は `std::string` に組み立てて
  から一度だけコピーし、`len-1` 文字での切り捨て・戻り値
  （末尾へのポインタ）・出力が埋まった時点で走査を止める挙動も
  従来通りです。数値書式は同じ `snprintf` 書式を使っています。
- `cmtstrcpy()` は同じアルゴリズムを添字ベースで書き直し、
  コメントの数え方・長さの扱い・未完コメントの巻き戻しを
  コメントで説明しました。`nstr()` は `std::fill_n` 化、
  `dmcheck_write()` は `std::array` 化しました。
- `%s` に NULL を渡すと従来は落ちていました。空文字として扱う
  ようにしました（落ちる経路以外に影響はありません）。
- この書き換えにより、このファイルの `-Wwrite-strings` 警告が
  無くなりました。

### 次のモジュール

予定通り、小さい拡張群（`ext_party.cpp` / `ext_data.cpp` /
`new_command.cpp`）が次です。その後 `new_proto.cpp` →
`phi_m57.cpp` → `expression_op.cpp` → `phi_calc.cpp` と進みます。

## 第2段階：小さい拡張群の書き換え（2026-11-15）

`ext_party.cpp` / `ext_data.cpp` / `new_command.cpp` の3モジュールを
C++ らしく書き換えました。公開 API（ヘッダ）と `phi_dm.h` の
グローバル構造は変更していません。検証は第1段階と同じく、決まった
手順のクライアントセッションを書き換え前後のバイナリで流して
比べる方法で行いました（テスト用ワールド、ポート20019、スナップ
ショットから毎回復元）。単独セッション（ログイン・look・cast・
bye・join・show 3種・skill・guard・ゲート移動）と、2クライアント
のパーティセッション（招待→受諾→結成→離脱→解散）の両方で、
正規化テキストが前後一致しました。全文差分に残ったのは `#m57`
の敵位置（ランダム移動）のみです。検証基盤は
`~/workspace/phi-cpp-verify/`（プローブ・スナップショット・
前後バイナリ・差分テスト）に残してあり、後の段階でも使えます。

### ext_party.cpp

- メッセージバッファを `std::array<char, 256>` に変更。
- `ask_join` の深いネストを早期分岐に平坦化しました（判断順序と
  送信順序は不変です）。
- 招待の状態機械（`ST_JOIN_INVITE` / `ST_JOIN_ANSWER`）と
  パーティ番号の振り直しに日本語コメントを追加しました。

### ext_data.cpp

- プロパティ文字列の読み書きです。ポインタ走査のアルゴリズムは
  バイト単位でそのままにし、構文とコメントだけ整えました
  （`nullptr`、`std::array` バッファ、`std::strcmp` / `strstr` /
  `snprintf` / `sscanf`）。`std::string` を解析の核に押し込むのは
  やめています（`loadstr` の切り捨て時のポインタ進行などの癖を
  保つためです）。
- `match_property` の「一致=-1・不一致=0」という約束、数値比較の
  `%i` の意味（`0x` / `0` 接頭辞、接頭辞だけの解析）、
  `set_property` の引用規則をコメントに残しました。
- セッションだけでは `match_property` / `set_property` の網羅が
  間接的なため、差分ユニットテストを追加しました。旧実装と新実装
  を別名でコンパイルし、`loadstr` / `wildcmp` / `fisspace` の
  verbatim コピーを共通ヘルパーとして、130件の入力（引用・
  切り捨て・NULL・ワイルドカード・数値比較・SJIS・`dsize==1`・
  src==dst エイリアスなど）で戻り値・出力バッファ・ポインタ進行
  を比較し、不一致0件でした。
- `set_property` の `dsize==1` の癖：何も写していない状態で
  `*(dst-1)` を読み、dst の1つ前のバイトの値で戻り値が変わります。
  書き換えは同じ式のままにしてあり、差分テストでもガード値を
  `' '` と `'x'` の両方で一致を確認しました。コメントに記録
  してあります。

### new_command.cpp

- 振り分けチェーンの形はそのままに `std::*` 化しました。
- `show_file` は固定バッファから `std::string` にし、256文字での
  打ち切りは同じに再現しました。従来は256バイト固定バッファで、
  許可文字が246文字以上あると末尾の `'\0'` が1バイトはみ出して
  いました。`std::string` 化で打ち切りを保ちつつ、はみ出しを
  無くしました（旧バイナリでこの経路を叩くのは危ないため、
  セッション比較では通常長の名前だけを使っています）。
- `bulletin_board_dm` は `unique_ptr<FILE, efclose>` の RAII にし、
  末尾の空行の前に `reset()` で閉じる順序を保ちました（`efclose`
  は `lcnt = -1` を設定するため `fclose` では代用できません）。
- `command_analyze_ex` のフォールスルー：`status_skills()` が
  0以外を返すと次の分岐へ落ち、最終的に1を返して発言扱いに
  なります。形を保ってコメントに残しました。
- `command_analyze_ex` で処理されるコマンド（join/bye/guard/
  show/skill）は `command_analyze()` の gasify 解除を通りません。
  パーティ検証では、招待を受ける側に事前に `turn` を送らせる
  必要がありました。

### 次のモジュール

`new_proto.cpp` → `phi_m57.cpp` → `expression_op.cpp` →
`phi_calc.cpp` と進みます。

## 第3段階：new_proto.cpp の書き換え（2026-11-15）

`new_proto.cpp`（拡張プロトコル `#ex-*` の処理）を C++ らしく
書き換えました。公開 API（`new_proto.h`）と `phi_dm.h` のグローバル
構造は変更していません。検証は第1・第2段階と同じく、決まった手順の
クライアントセッションを書き換え前後のバイナリで流して比べる方法で
行いました（テスト用ワールド、ポート20019、スナップショットから
毎回復元）。このモジュール専用のプローブ（`probe_newproto.py`）を
新しく作り、5種類のワールド設定（通常・`:F` 初期グラ一覧あり・
`:U 7 X`・`:U 7 T`・`:U 7 S`）で `#ex-map` / `#ex-switch` の諸値と
`#ex-register` の成功・拒否・`#ex-get` を踏み、さらに image 付き
登録キャラでの `#name` 送り直し（`probe_newproto2.py`）と、既存の
単独セッションプローブでも前後一致を確認しました。全文差分に残った
のは `#m57` / `#map` の敵位置・`#cond` の挿入位置・接続者一覧の
番号・`phi.cd` の経験値と時刻（いずれも実行ごとのゆらぎで、番号の
向きが前後で逆転することも確認）だけでした。登録で作られた `phi.cd`
の内容（ID の組み立て・名前の切り捨て・引用名・長いパスワードの
切り捨て）も前後一致しました。

### new_proto.cpp

- バッファを `std::array` にし、`std::strcmp` / `std::strncmp` /
  `std::snprintf` / `std::sscanf` / `std::strlen` 化しました。
  解析の核（`loadstr` / `get_property` への呼び出し、切り捨て時の
  ポインタ進行、`fisspace` の番兵チェック）はバイト単位でそのまま
  にし、構文とコメントだけ整えています（`ext_data.cpp` と同じ流儀）。
- `#name` の送り直しの3か所を、無名名前空間のヘルパー
  `send_name_if_cg_changed()` にまとめました。送る名前の違い
  （size=57 を受け入れたときだけキャラ名、それ以外はグラ名）は
  引数で残してあります。
- `ex_adduser_v52_finish()` の癖をコメントに残しました。pass は
  uid の9バイト目から空白までを切り詰めて6文字ちょうどでなけれ
  ば拒否（`%-6.6s` の空白埋めと合わせ、6文字ちょうど・空白なしの
  パスワードだけが通る）。`f_mail` が立つことはなく、拒否文の
  mail 欄は常に空。拒否文の書式自体に末尾の改行は無いが、
  `check_write()` が補う。
- 書き換え中に見つけた読み違えやすい点：元の拒否文の
  `" name"` の直後のタブ文字は、三項演算子の `:` の前の空白で
  あって文字列の一部ではありません。バイトで確認し、書き換え後も
  同じ文字列になるよう直しました（最初は文字列内のタブと読んで
  しまい、検証前に気づいて修正）。文字列リテラルは機械的に照合
  済みです（ヘルパー化による集約とコメント内の引用を除き一致）。
- `style` の未知の値では返事の後に即 return し、残りのプロパティ
  を処理しないこと、`size` の未知の値では返事をせずに地図だけ
  描き直すこと、`ex_notice()` の landkey (f=2) が未実装のまま
  であることをコメントに残しました。
- 警告の数は書き換え前後で同じ（14件、いずれも従来からの
  `-Wwrite-strings`）です。

### 次のモジュール

`phi_m57.cpp` → `expression_op.cpp` → `phi_calc.cpp` と進みます。

## 第4段階：phi_m57.cpp の書き換え（2026-11-15）

`phi_m57.cpp`（`#m57` 地図と Eagle eye の広域地図の生成）を C++
らしく書き換えました。公開 API（`phi_m57.h`）と `phi_dm.h` の
グローバル構造は変更していません。地図はバイナリ形式のため、出力
バイトの同一性を最優先にし、文字列リテラル・文字リテラル・走査
順序の4配列を機械的に照合済みです（いずれも書き換え前後で一致）。

### phi_m57.cpp

- バッファを `std::array<char, 256>` にし、`std::memcmp` /
  `std::memcpy` / `std::strcmp` / `std::snprintf` / `std::abs` /
  `std::time` 化しました。`NULL` は `nullptr` にし、再代入しない
  変数は `const` にしました。
- 視界 7x7（`view7`）・前回視界（`last_view7`）・前回マップセット
  （`last_mapset`）・`mapset` / `mapuse` / `obj_start_num`・方向
  表（`dc` / `rdc`）を `std::array` にしました。走査順序の4配列
  （`tsx` / `tsy` / `ssx` / `ssy`）は `constexpr std::array` にし、
  ポインタ進行のループを添字のループに変えました（順序は不変）。
- `last_npc` だけは C 配列のままにしました。`CPYSTR` が
  `strlcpy` と `sizeof` に依存しており、`std::array` の
  `.data()` を渡すと `sizeof` がポインタの大きさになって行が
  途中で切れるためです。理由をコメントに残してあります。
- 背景オブジェとエフェクトオブジェの2つの走査は、ほぼ同じ形の
  まま残しました。関数静的なキャッシュに触れるためヘルパー化
  すると見通しが悪くなること、違う点が3つだけ（効果フィルタの
  向き・行頭の文字 B/F・末尾の種類 OT_STATUE/OT_MAGIC）である
  ことをコメントに残してあります。
- `eagleeye_m57` の文字列リテラル2か所に `(char *)` のキャスト
  を付け、このファイルの `-Wwrite-strings` 警告 2 件を無くしま
  した。`check_write()` は引数を別バッファへ写してから送り、
  書き換えないことを確認済みです（第0段階と同じ流儀）。全体の
  警告数は 1806 件から 1804 件になりました。
- 保った癖をコメントに記録しました。`is_visible` の斜めの例外
  （`d != 3`）と `d > 2` のガード、近いセルから埋める順序が
  前提であること。`set_view7` の盲目処理（`blind < 0` でも
  `i == 0` の立ち位置は見える）。マップセットの重複判定が
  ポインタ比較であることと、4組を超えたセルは0番になること。
  `dd_m57` の2バイト目の配置（`1iiimraa`）と、キャラ存在印
  `'X'` が2バイト目全体を置き換えること、`p` は値渡しで進ま
  ないこと（従来の「バッファのポインタは進められる」という
  コメントは古い）。`print_m57` の2段の差分判定（視界全体の
  比較と、行頭6バイトを除く行ごとの比較）と、`reflesh_objects`
  のときに差分判定を飛ばして前回の途中から走査すること。
  `eagleeye_m57` の半径式と、`#endif` を挟んだ `else` の結び
  付き（NO_SKILL 未定義のとき半径判定の `if` に結び付く）。
  `#m57 M` 行頭の18バイトと末尾の位置、`#ex-eagleeye M` 行頭の
  21バイトもコメントに残しました。

### 検証

- このモジュール専用のプローブ（`probe_m57.py`）とバイト比較
  （`compare_m57.py`）と実行台（`run_m57.sh`）を新しく作り
  ました。プローブは生バイトを保存し、ゲストでゲート移動・
  turn / go・`style=turn` / `solid` の切り替え・check を、
  master で `cast` → `Eagle eye` を踏みます。`#m57 M` 行は
  時刻トークンだけを潰して比較し、`#m57 O` 行は自キャラと
  オブジェを比較（状態バイトと接続スロット番号は実行ごとに
  変わるため潰す）、`#ex-eagleeye M` 行はキャラ存在印を潰して
  比較します。
- 敵のランダム移動が地図バイトに及ぶ（床アイテムの拾得・キャラ
  存在印・1セルの地形）ため、敵のいないワールド設定（`phi.ed`
  の発生数を 0 にしたもの）を用意し、決定論的な比較ができる
  ようにしました。通常設定と `:U 7 C`（PSP互換）の両方で、敵
  なし・敵ありの4通りを比べています。
- 敵なしの通常設定と `:U 7 C` 設定で、書き換え前後が完全一致
  しました（各2回ずつ。地図バイトとテキスト115行の両方）。
  敵ありの設定では、書き換え前後で1〜2セルの差が出ますが、
  書き換え前同士の比較でも同じ位置・同じ種類（キャラ存在印・
  アイテム・地形の1セル）の差が出ることを確認し、回帰でない
  ことを切り分けました。
- 既存の単独セッションプローブと2クライアントのパーティ
  プローブでも、正規化テキストが前後一致しました。
- 動的に踏めていない経路があります。このワールドには配置済みの
  オブジェが無く（`phi.od` の `:O` はすべて未配置）、視界内の
  マップセットも1組だけのため、オブジェの B/F 行と
  `#mapset-define` 行は一度も送られませんでした。これらの経路
  は書式文字列と引数の照合（機械照合に含む）で確認しています。
  `MAX_OBJECTS == 0` や `NO_SKILL` 定義時のビルドは従来通り
  未確認ですが、プリプロセッサの構造はそのまま保ってあります。

### 次のモジュール

`expression_op.cpp` → `phi_calc.cpp` と進みます。

## 第5段階：expression_op.cpp の書き換え（2026-11-15）

`expression_op.cpp`（スイッチデータのアクション 50「式計算」の
コンパイルと評価）を C++ らしく書き換えました。公開 API
（`expression_op.h`）と `phi_dm.h` のグローバル構造は変更して
いません。式の評価結果・副作用・エラー時の挙動の同一性を最優先
にし、文字列リテラルの多重集合・文字リテラルの多重集合・
`ERRM_*` マクロの呼び出し順序（33 か所）・システム変数テーブルの
順序を機械的に照合済みです（いずれも書き換え前後で一致）。

### expression_op.cpp

- インクルードを `<array>` `<cstdio>` `<cstdlib>` `<cstring>`
  `<ctime>` `<string_view>` にしました。未使用の
  `<sys/time.h>` / `<stdlib.h>` は外し、`<cstdlib>` は
  `rand()`（`frand` マクロ経由）のために残しました。モジュール
  全体の概要コメントを先頭に付けました。
- 無名名前空間に `log_error` ラッパー（`log_print` へ
  `const_cast<char*>` で渡す。`check_write()` は引数を別
  バッファへ写してから送り書き換えないことを確認済みで、第4
  段階と同じ流儀）と、評価スタック
  `std::array<ACUM, MAX_STACKS> stack` と、システム変数表
  `kSysVars`（12 項目、元の if-else 連鎖と同じ順序。`wpn` と
  `player` の歴史的コメントは表に残した）を置きました。
  `stack` はこのファイルの中でしか参照されないことを全ツリー
  の grep で確認してからファイルローカルにしました。
- `stpt` は素の `int` グローバルのままにしました。
  `phi_data.cpp` が評価の直前に `stpt = 0;` でリセットするため
  です。`get_sentence` の `char buf[256]` は
  `std::array<char, 256>` にし、`.data()` / `.size()` で渡し
  ました。
- `get_token` の `$` システム変数の if-else 連鎖だけを表引きの
  ループに変えました。他の要素名の連鎖は `std::strncmp` +
  `log_error` 化するだけに留め、順序と条件を変えていません。
- `get_sentence` の制御の流れ（`goto`・フォールスルー・
  `recall` / `termnext`）は逐語的に保ちました。要素やオフ
  セットの式、`[` の境界判定の符号混じり比較、`get_value` の
  型の癖、`=` のデフォルトケースへのフォールスルー、
  `FUNC_RND` の `put_stack` 結果の無視、`EXP_SYSVAR` の
  `EXP_CONST` へのフォールスルーは、すべて保ってコメントに
  記録しました。
- `calc_expression` の外側の `int i`（`gp` の旧値を保持する
  だけの変数）は `old_gp` に改名し、`'A'` / `'D'` のループの
  `int i` はループローカルにしました。`NULL` は `nullptr` に、
  `sscanf` / `time` は `std::sscanf` / `std::time` にしました。
- このファイルの警告は 34 件から 2 件になりました（残りは
  `phi_dm.h` 由来の `-Wliteral-suffix` 1 件と、符号混じり比較
  を意図的に残した `-Wsign-compare` 1 件）。全体の警告数は
  1804 件から 1772 件になりました。

保った癖のうち主なものを、コメントにも記録しました。

- 優先順位は2段階で、出力は逆ポーランド記法のため低位の演算子
  は右結合です（`10-2-3` は 11）。`,` は `EXP_RESET` を出して
  続けます。複合代入 `op=` は `OP_STACKTOPCOPY` を先に出して
  `x=x+1` と同じ形にします。`!` 単独と `!=` はどちらも不等号
  です。
- `f` フラグは `default:` の場合にだけ立つため、括弧でくくった
  項の直後の `+` / `-` は単項として読まれ、二項の `+` / `-` は
  飲み込まれます（例：`@CHR.gp=-5+3` は `5 REVERSE 3 =` に
  コンパイルされ、実行時に `ERRM_ILLEAGAL_SUBST` になります）。
- `static int recall` と `termnext` により、低位の演算子のための
  再帰呼び出しは終端子を消費せず `TERMINATE` も出しません。
  括弧グループは終端子を出しません（最上位の `term='\0'` だけ
  が行末で出します）。
- 配列添字は1始まりで、素の16進形式は値+1 を格納し、`[式]` 形
  式は評価時に1を引きます（`x0` と `x[1]` は同じ）。16進数字は
  小文字の a〜f のみで最大8桁、10進は最大10桁です。
- `get_value` は変数のとき `type = EXP_CONST`（0x01）にします
  （`kind` は `EXP_VARIABLE` のまま）。`=` は左辺を生のまま
  `get_stack` で取り、NULL の `ADDRESS` では -1 を返し、未対応
  の型ではエラーを記録してから変数エントリを push する
  フォールスルーをします。`=` の副作用は `gp` なら `tax` の
  調整、`x` / `y` なら `clear_chr`・書き込み・`set_chr`・
  `print_map` と `bx` / `by` の同期（`trnf` のとき除く）、
  `gvar` / `pvar` / `svar` なら `gfbuf2` へのフラグ記録です。
- `@Book` は `get_magicelement` ではなく `get_itemelement` を
  使います。`prm` は `op[1..3]`（3要素）です。`$pay` /
  `$talk` は `sscanf(cd[n].str, "%i", ...)` で、一致しないと
  `VAL` は元の共用体のゴミのままです。`$time` は
  `time(NULL)`、`$match` は `match_n` です。
- **既存のクラッシュバグ（保ったままにしています）**：
  `OP_REVERSE` は `get_value` のあと `a2.VAL = -a2.VAL` とし、
  `kind` を `EXP_VARIABLE` のまま push します。`VAL`
  （`val.di`）と `ADDRESS`（`val.dp`）は `ATOM` 共用体を共有
  するため、否定した値が `ADDRESS` の下位 32 ビットを上書き
  し、次の `get_value` がその値をポインタとして参照して
  SIGSEGV になります（故障アドレスの下位 32 ビットは `-1000`
  の `0xfffffc18` と観測）。引き金は変数・要素参照への直接の
  単項マイナス（`-(x1)`・`-x1`・`-@CHR.gp` など）で、定数・
  システム変数・関数結果・括弧でくくった算術への単項マイナス
  は安全です。修正（`REVERSE` の場合に `kind = EXP_CONST` に
  する）は振る舞いを変えるため、別途の判断が必要な先送り事項
  とし、クラッシュの等価性を variant C で確認しました。
- スタックあふれは `phi.swt` 経由では到達できません。
  `ATOMBUF_SIZE`（500）がコンパイル済みコードを抑えるため、
  push できる定数は最大で約245個で `MAX_STACKS`（256）に
  届かず、`ERRM_STACK_OVERFLOW` は実質的に死にコードです。
  `put_stack` の検査はそのまま残しました。
- `SKIP_SPACE` は `' '` と `'\t'` のときにだけ真です
  （`fisspace` の定義）。`get_codetoken` には境界検査が無く、
  終端子の無い不正な式はヒープを読み過ぎます。そのため、
  発火するテストには終端子を持つ式だけを使いました。

### 検証

- このモジュール専用のプローブ（`probe_expr.py`：`walk` /
  `still` の2モード）と実行台（`run_expr.sh`：ポートの完全な
  解放を待ってから起動）とバイト比較（`compare_expr.py`）と
  4種類の `phi.swt`（variant A / B1 / B2 / C）を新しく作り
  ました。プローブは各セルに置いたスイッチが結果を
  `@CHR.gp` へ代入し、`check` の所持金行で観測します。
- テストワールドの地図の読み方を訂正しました。`phi.dd` の行は
  32文字ですが `load_dd` は17文字ずつ読むため、ファイルの
  0〜3行目だけが読み込まれます。guest の出現位置は (1,2) で、
  (1,1) はゲート、(3,6) と (5,6) は壁です。歩行経路はこれを
  避け、各セルのスイッチが到着時にちょうど1回だけ発火する
  ことを確認しました。
- variant A（正常な式 31 個 + フラグスイッチ 1 個）：書き換え
  前後で正規化テキスト 520 行が完全一致、phi.log 11 行が一致、
  GP 列は 32 個のうち 31 個が完全一致しました。残る1個は
  `@CHR.exp` を読むステップで、経験値が時間経過で +1 される
  ため実行ごとに変わることを書き換え前同士の比較で確認し、
  マスクしています。`rnd()` の値も実行ごとに変わること
  （73 と 12）を書き換え前同士で確認し、範囲と実行内での
  安定性だけを確かめています。HP の回復も時刻依存のため厳密
  比較していません。
- `$time` の扱い：`$time` の代入は `gp = time(NULL)` になり
  ますが、直前のテレポートで (3,3) へ移動したあと、次の
  tick で (3,3) のスイッチが再発火して `gp` を 43
  （`$totalwp` 42 + `$player` 1）に決定的に上書きします
  （`:O MSGT` の `T` によりテレポート時の `bx` / `by` 同期が
  省かれるため）。`tax` への影響も `$time` が相殺されて
  決定的になります。
- variant B1（安全な実行時エラー 11 件：ゼロ除算・範囲外添字・
  未装備・範囲外要素・途中での中断）：正規化テキスト 521 行
  と phi.log 20 行が前後一致し、`swt err` は両方 11 件でした。
- variant B2（コンパイルエラー 10 件：未定義の変数・要素・
  不正な文字列・括弧の不一致・長すぎる式。発火しないセルに
  配置）：正規化テキスト 68 行と phi.log 20 行が前後一致
  しました。
- variant C（クラッシュの等価性：`-(x1)` を (1,2) に配置）：
  書き換え前後ともログイン直後の tick で SIGSEGV になり、
  部分的な記録 44 行と phi.log 9 行（`SIGSEGV  : Outer God`
  を含む）が一致しました。
- 既存プローブでの回帰確認：2クライアントのパーティプローブ
  は A/B とも正規化テキストが前後一致、敵なしの `#m57`
  プローブは地図バイトとテキスト 115 行が前後一致しました。
  単独セッションプローブは Cat の `#m57 O` 行の位置・向き
  だけが前後で異なり、書き換え前同士の比較でも同じ種類の
  差が出ることを確認して回帰でないと切り分けました。
- 調査の過程で、ASan ビルドにより `load_dd()`（`phi_data.cpp`）
  の `yname` を1バイトずらす `memcpy` の重なりを発見しました
  （ASan が起動時に異常終了する）。`phi_data.cpp` の段階の
  先送り事項とし、このモジュールでは直していません。ASan
  での調査には `ASAN_OPTIONS=replace_intrin=0` が要ります。

### 次のモジュール

`phi_calc.cpp` → `phi_use.cpp` と進みます。

## 第6段階：phi_calc.cpp の書き換え（2026-11-15）

`phi_calc.cpp`（ダメージ・死亡・レベルアップ・数値解析などの
計算系）を C++ らしく書き換えました。公開 API と `phi_dm.h`
のグローバル構造は変更していません。計算結果・副作用・
エラー時の挙動の同一性を最優先にし、文字列リテラルの
多重集合（17 個）・文字リテラルの多重集合（53 個）を機械的に
照合し、コメントと空白を除いたトークン差分が意図した変更
だけであることを確認済みです（いずれも書き換え前後で一致）。

### phi_calc.cpp

- インクルードを `<array>` `<climits>` `<cmath>` `<cstdio>`
  `<cstdlib>` `<cstring>` にしました。未使用の `<ctype.h>`
  は外し、未使用の extern 宣言 `px[]` / `py[]` /
  `parsecom[]` も削除しました（`X_FORE` / `Y_FORE` は
  このファイルで使われていません）。モジュール冒頭に
  解説コメントを追加しました。
- `dead()` の死亡ペナルティ上限の if-else 連鎖を、
  `kLevelLimits` テーブル（5 段：1000/50・3000/100・
  5000/200・7000/500・INT_MAX/1000）のループにしました。
  境界と順序は同じで、`limit_nlv` は INT_MAX の番兵により
  常に代入されます。lv 0..100000 で全数照合し一致しました。
- `lv_up()` の三役変数 `i`（しきい値→40000 超えフラグ→
  減少ループ添字）を分割し、フラグを `over_limit` に
  改名しました。評価順序は変えていません。
- `gigant_check()` の同一の 2 分岐を 1 つの条件
  （`mhp >= 5000 && (type != CT_NORMAL || ggflag)`）に
  統合しました。真理値表で全組み合わせの等価性を
  確認済みです。
- `fn()` の `static int fn_rate[6]` を
  `static constexpr std::array<int, 6>` に、`fisqrt()` の
  `double d[5]` を `std::array<double, 5>` にしました。
- `se2ef`（状態異常→効果の対応表 35 要素）を
  `constexpr std::array` にして無名名前空間へ移動しました
  （他ファイルからの参照が無いことを grep で確認済み）。
- `char buf[256]` を `std::array<char, 256>` にしました
  （`dead` / `raise_dead` / `kill_other` /
  `kill_self_msg`）。`snprintf` / `strlen` / `strcmp` /
  `log10` は `std::` 版に、`NULL` は `nullptr` にしました。
- `hp_dmg` / `mp_dmg` はロジックを完全に維持しました
  （`PER_DICE` の評価回数と短絡評価の順序を含みます。
  トークン差分は空白のみ）。`mp_dmg` のコメントアウト行の
  入れ子 `/*` を直し、ファイルの `-Wcomment` を解消しました
  （コメントだけの変更です）。
- `analyze_numbers` / `analyze_single_number` /
  `analyze_hexnumbers` はポインタ走査のアルゴリズムを
  逐語維持しました（第 2 段階の `ext_data.cpp` と同じ方針）。
  癖はコメントに記録してあります。
- `set_fn` / `ecn` / `sep_calc` / `sg_check` / `sum_wp` /
  `df_rate` / `mr_rate` / `dice` / `lv_dice` /
  `cast_mmpup` は nullptr 化と `std::` 化のみです。

### 見つけた癖（すべて維持、コメントに記録）

- `hp_dmg` の f==2（魔法暴発）の成長式は `mhp` / `hp` を
  使い、`mmp` / `mp` ではありません（コピペ由来と
  思われます）。
- `mp_dmg` の f==1 の成長ブロックは詠唱中断チェックの
  内側にあります（`hp_dmg` では兄弟位置）。50 倍式で
  `mmp` / `mp` を使います。`mp_dmg` の f==2 は MP を 1
  で止め、死にません。`mhp` / `mmp` が 0 のときの
  ゼロ除算は短絡ガードで回避されます。
- `dead()`：exp==99999999 の PC 分岐は全回復・ペナルティ
  無しですが GP を全額税へ送ります。`tax += gp - gp/2`
  のあと `gp /= 2` で、税は切り上げ側の半分を受け取ります。
  `mhp` / `mmp -= 2` のあと exp<1440 で 1000 に床、
  さらに <1 でクランプする順序を維持しました。
  `close_connect(n)` は NPC でも必ず呼ばれます。
- `lv_up()`：switch の `default:` は減少ブロックを通らず
  return します。40000 超えの均しは、他に正のレベルが
  無い場合に今上がったレベル自身を下げます。`pc` は
  0/1/2（両 PC でない／両 PC で IP が違う／同じ IP）です。
- `lv_dice`：`RAND % (lv+1) / 100` は lv ≤ 99 では常に 0
  です。
- `fn()`：添字 `i` は `afn` / `dfn` が FN_NONE..FN_RAINBOW
  (0..9) の範囲内のときだけ 0..5 です。データ由来の
  範囲外値は境界外読みになります（潜在的な問題として
  残しています）。
- `debug_buf` は `#ifdef DEBUG` で `phi_calc.cpp` /
  `phi_dm.cpp` / `phi_m57.cpp` に定義されますが、
  `kill_other` は `#ifdef DEBUG_ANGRY` で使います。
  DEBUG_ANGRY のみのビルドは壊れますが、ビルドで
  DEBUG は定義されないため実害はなく、未変更です。
- `analyze_numbers` と `analyze_hexnumbers` の非対称：
  10 進は非増加を拒否（`k <= num` → 0）しますが、16 進は
  等号を許します（`k < num`）。16 進は小文字 a-f のみを
  受け付け（大文字は区切り）、末尾に `-1` を無条件で
  書くため呼び出し側は size+1 の配列が必要です（唯一の
  呼び出し側は `ter[257]` に size 256 で安全です）。10 進の
  `-` 単独は `1, -1` を格納します（size ≥ 2 が必要。
  呼び出し側は 16 要素で安全です）。
- `ecn` は `IT_MASK` (0xf) と同じ値のリテラル `0xf` を
  使います（表記の不統一のみです）。
- `se2ef` の添字は `SE_NONE < SE <= MAX_SE` (34) で
  guard され、35 要素の 1..34 が有効です。

### 検証

- ビルド：`g++ -std=c++17` で `make` と `make cdcnv`
  が通り、`-fcommon` は不要です。単一ファイルの警告は
  18 → 17 になりました（`-Wcomment` の解消。残りは
  `phi_dm.h` / `dm_msg_jp.h` 由来と `check_write` の
  2 件で従来通りです）。
- 機械照合：文字列リテラル多重集合（17 個）・文字リテラル
  多重集合（53 個）が前後一致しました。死亡ペナルティ表は
  lv 0..100000 で全数一致、`gigant_check` は全組み合わせで
  一致、`fn()` の添字は afn/dfn 0..9 の全組み合わせで
  0..5 の範囲内でした。
- 新規プローブ `probe_calc.py`（guest、約 74 秒：
  look/check/spells/cast/equip/unequip/hit/guard/skill/
  sort/board/get/put/use/pay/show/turn/go/ゲート移動）を
  書き換え前 2 回・後 1 回実行しました。スクリプト応答は
  3 回とも同一で、残る差は敵の徘徊・戦闘チャットのみでした。
  書き換え前同士の比較でも同じ種類の差が出ることを確認し、
  敵チャット行を除いても書き換え前同士で 1 行の `hp left.`
  差が残る（同じランダム要因）ことを確認して、回帰でないと
  切り分けました。後バイナリの実行では kill 行も出て、
  `kill_other` / `dead` 経路が動くことを確認しました。
- 単独セッションプローブ（`probe_single.py`）：正規化
  テキストが前後完全一致しました。
- `#m57` プローブ：敵なし variant n は地図バイト
  （`#m57 M` 25 行・`#m57 O` 自キャラ 12 行・
  `#ex-eagleeye M` 15 行）とテキスト 115 行が前後完全一致
  しました。敵あり variant base は書き換え前同士でも
  同じ箇所（`#m57 M` の 1 セル・eagleeye の 3 セル・
  敵チャット）が異なり、前後比較の差分は書き換え前同士の
  差分と同一（before2 と after は同一出力）で、回帰でない
  ことを確認しました。
- 実サーバ 2 台（20017/20018）には触れていません。
  テストサーバ（20019）は各実行後に PID で停止済みです。
- 検証基盤の注意点：`run_probe.sh` / `run_m57.sh` は
  テストワールドのディレクトリへ cd するため、プローブ
  スクリプトと出力パスは絶対パスで渡す必要があります
  （相対パスだと出力が失われます）。

### 次のモジュール

`phi_use.cpp` → `ext_skills.cpp` と進みます。

## 第7段階：phi_use.cpp の書き換え（2026-11-15）

`phi_use.cpp`（アイテム使用・チャージ・罠・アイテム管理）を
C++ らしく書き換えました。公開 API と `phi_dm.h` のグローバル
構造は変更していません。文字列リテラルの多重集合（9 個）・
文字リテラルの多重集合（0 個）を機械的に照合し、書き換え前後で
一致することを確認済みです。

### phi_use.cpp

- インクルードを `<array>` `<cstddef>` `<cstdio>`
  `<cstdlib>` `<cstring>` `<ctime>` にしました。未使用の
  `<ctype.h>` / `<math.h>` と `#ifndef Mips` ガードは外しました。
- ファイルローカルの extern 宣言 `enmax` / `ed_o` を削除
  しました（`phi_data.h` で宣言済み）。`px[]` / `py[]`
  はどのヘッダにも宣言が無いため extern 宣言を残しました。
- レベルエンハンサー（IT_FOOD の type 0xa000〜）の 6 分岐
  は、対象フィールドとメッセージ以外が同一だったため、
  無名名前空間のテンプレート `apply_enhancer` にまとめました。
  VALUE==99 なら ENTROP を加算、それ以外は減算し、
  [1, 9999] で切り詰めます。`lv[]` は int、`mhp` / `mmp`
  は long のため、加算結果をフィールドへ代入してから
  （int への narrowing を含む）比較する元の順序を保ちます。
  分岐条件（末尾の `type >= 0xf000` に上限が無いことも含む）
  と `break;` は元のままです。
- 資源（IT_RESOURCE）の UE switch のうち、UE 1〜9 の 9 種
  （地形ビットの置換 + メッセージの同一手順）を
  `constexpr std::array` の表 `kResourceTerrains` にしました。
  RS_RESOLVER (0) は `load_id_in_circle()` を伴い、
  RS_ROOF (10) はトグル動作のため小さな switch に残し、
  RS_DUMMY (99) を含むその他は無操作の default にしました
  （元の switch の case 不一致と同じ）。
- `charge_appear()` / `charge_disappear()` で同一だった
  敵スロット消滅の塊を、無名名前空間の
  `clear_enemy_instance()` にまとめました。消滅させたら
  true を返し、disappear 側の `k` カウントに使います。
  appear 側は戻り値を捨てます（元も数えていません）。
  未使用だった局所変数 `i` は削除しました。
- `char buf[256]` / `buf2[256]`（`use_item`）と
  `char buf[100]`（`use_charge`）、`int cbuf[MAX_ENEMIES]`
  （`charge_appear` / `charge_disappear`）を
  `std::array` にしました。`NULL` は `nullptr` に、
  `time()` / `free()` は `std::` 版にしました。
- `effect_trap()` のポインタ操作・CHARGE 減算・床アイテム
  リストの切り離しは逐語維持しました（`nullptr` /
  `std::free` 化のみ）。`delete_item()` /
  `item_count()` / `itemtype_count()` はコードを変更せず、
  癖のコメントだけを追加しました。

### 見つけた癖（すべて維持、コメントに記録）

- `effect_trap` の EF_MINEHP には `break` が無く、
  EF_MINEMP へフォールスルーして両方の爆発が起きます。
  コメントで明示し、コードは元のままです。
- `charge_appear` は一致した定義数の合計 `m` を数えますが、
  使われません。
- 食べ物の「lucky」判定はキャラクター名の先頭文字
  （`(*cd[n].name % 49) + 50`）を使い、アイテム名では
  ありません。
- エンハンサーの末尾分岐は `type >= 0xf000` で上限が無く、
  加算結果はフィールドへ代入（narrowing を含む）してから
  クランプ比較します。
- `use_charge` の劣化条件は短絡評価の順序に `degrade()`
  の副作用が依存します。`is_use_fail`（出現・消滅の不一致）
  と罠設置の分岐は CHARGE 減算の前に return します。
  `active_time = time(NULL) + frand(mgstrlen(name))` は
  乱数を消費します（乱数列の一致のため順序を維持）。
- `effect_trap` の `MagicData md` は未初期化のまま部分代入
  されますが、`effect` は各 case で設定してから使い、
  いずれの case にも一致しなければ読まれません。
  罠は 1 歩につき最初の 1 個だけ発火します。
- `delete_item` は type/wp/fn/ATP/ER/AE/SE/active_time/
  extension を初期化しますが、name や op[] / CHARGE は
  消しません。
- `itemtype_count` は DT_NONE の空きスロットも数えます
  （`0xf00 & 0xf == 0 == IT_NOUSE` のため、it==IT_NOUSE
  のとき空きも一致します）。
- 資源の MISS はアイテムを消費します（if/else の後ろの
  共通末尾へ落ちます）。NO / OUT / RESOLVE_NO は
  return 0 で消費しません。RS_DUMMY は無言で消費されます。
- IT_EVENTLY は BCD→16 進のフラグ変換をし、op[2] が 0 に
  なるまで消費しません（op[2]==0 なら保持、op[2]==1 なら
  0 にして消費）。
- IT_SCROLL は ST_MSGW と `anmf = op[1]*100 + op[2]` を
  設定します。メッセージ 203 が無いワールドでは
  `load_mesg` が anmf を無言で消し、ST_COMMAND に戻ります。
- IT_ACCESSORY の `weq == item` の二重装備防止分岐は、
  通常の装備経路では到達しません（逐語維持）。
- `charge_appear` / `charge_disappear` はインターン済み
  文字列のポインタ比較（`apnm` / `apdm` と `id->name`）
  で一致判定します。
- ログイン時の `auto_equip` は防具・装飾品以外をすべて
  武器候補とします（`phi_dm.cpp` の挙動。プローブ設計に
  影響しますが、このファイルの変更対象ではありません）。

### 検証

- ビルド：`g++ -std=c++17` で `make` が通り、
  `-fcommon` は不要です。単一ファイルの警告は 67 → 46
  になりました（エンハンサーの 12 か所と資源表の 9 か所が
  `const_cast` のヘルパー経由になった分だけ
  `-Wwrite-strings` が減りました。残りの符号比較 1 件と
  リテラル接尾辞 1 件は書き換え前からあります）。
- 新規プローブ `probe_use.py` の 12 変種（a / a2 / b /
  c1〜c5 / d / d2 / d3 / e：装備・食べ物・毒・lucky・
  エンハンサー・呪文書・資源 10 種・チャージ・罠 4 種・
  出現/消滅の一致）を書き換え前 2 回・後 1 回実行しました。
  正規化全文（計 1401 行）が前後完全一致し、メッセージ断定
  と構造値（b の最終レベル F=1/W=7/M=4/C=6 と
  mhp=1009/mmp=1008、a2/c5 の GP 列の最終 1150）も一致
  しました。書き換え前同士も全変種一致し、プローブの
  安定性を確認しました。比較スクリプトの断定は、文面中の
  `/*color=*/` タグを除いて探すよう直しました
  （タグを挟む文面に素通しだったため）。
- 単独セッションプローブ（`probe_single.py`）：正規化
  テキスト 166 行が前後完全一致しました。
- `#m57` プローブ：敵なし variant n は地図バイト
  （`#m57 M` 25 行・`#m57 O` 自キャラ 12 行・
  `#ex-eagleeye M` 15 行）とテキスト 115 行が前後完全一致
  しました。
- パーティプローブ（`probe_party.py`）：要約 JSON が一致し、
  正規化 transcripts（A 77 行・B 78 行）も前後完全一致
  しました。
- phi.log：変種 b は時刻 prefix を除いて前後完全一致
  しました。変種 a の差は終了時系列のタイミング由来
  （`cli-err` 行と `data files are back up...` 行）で、
  書き換え前同士でも同一の差が出ることを確認し、回帰でない
  と切り分けました。
- 動的カバレッジの限界（逐語維持＋レビューで担保）：敵実体
  ありの消滅ブロック、資源の拡張ブロック
  （RSCOBJ/RSCSFC）、資源の MISS 経路、エンハンサーの
  上限クランプ 9999 は、決定的なプローブが作れないため
  動的には未カバーです。共有ヘルパー
  `clear_enemy_instance` は元の塊の逐語転写で、差分レビュー
  で確認しました。
- 実サーバ 2 台（20017/20018）には触れていません。
  テストサーバ（20019）は各実行後に PID で停止済みです。

### 次のモジュール

`ext_skills.cpp` と進みます。`load_dd()` の `memcpy`
重なりは `phi_data.cpp` の段階で対応します。

## 第8段階：ext_skills.cpp の書き換え（2026-11-16）

`ext_skills.cpp`（スキル表示・習得・劣化・保存/読込）を
C++ らしく書き換えました。公開 API（`ext_skills.h`）と
`phi_dm.h` のグローバル構造は変更していません。
文字列リテラルの多重集合（92 個）・文字リテラルの
多重集合（22 個）を機械的に照合し、書き換え前後で
一致することを確認済みです。

### ext_skills.cpp

- インクルードを `<array>` `<cmath>` `<cstdio>`
  `<cstdlib>` `<cstring>` にしました。
- ファイルローカルの表を無名名前空間の
  `constexpr std::array` にし、名前を k 前置にしました：
  `skl_max` → `kSkillMax`（`SKL_SPECIAL` の上限 0 を
  含む 14 要素）、`skl_id` → `kSkillId`、
  `skl_mgc_regions` → `kSkillMagicRegions`、
  `skl_mgc_cmnspl` → `kSkillMagicCommonSpells`。
  要素が `const char*` になったため `-Wwrite-strings`
  の大半が消えました。
- `skill_init` の 2 つの malloc + 埋めループ
  （PC は -1 埋め、敵は共有 `npc_skill` を 5000 埋め）を
  無名名前空間のヘルパー `alloc_skill_row(int**, int)`
  にまとめました。確保は `malloc` のままです
  （`skill_crt_check` の範囲外読みがヒープ配置に
  依存するため、配置を変えないことが重要です）。
  確保失敗時の部分状態も元のままです。
- `skill_clear` / `load_skills` / `save_skills_cd` /
  `put_skills` の `char buf[256]` を
  `std::array<char, 256>` にし、`sizeof(buf)` で
  渡すサイズ（256 / 231）は従来と同一にしました。
- 未使用の `extern int enmax;` を削除しました
  （`phi_data.h` で宣言済みで、このファイルでは
  未使用）。`lcnt` / `pfname` / `special_com` は
  どのヘッダにも宣言が無いため extern 宣言を
  残しました。
- `status_skills` の `buf` / `buf2` は C 配列のまま
  です（`CPYSTR` / `CATSTR` が `sizeof(d)` に
  依存するため）。
- `NULL` は `nullptr` に、`snprintf` / `strlen` /
  `strcmp` / `log10` / `malloc` 等は `std::` 版に
  しました。`strlcpy` / `strlcat` は無修飾のままです。
- `skillup` 系・`skill_down` 系・`skill_mg_calc`・
  `skill_crt_check`・`check_lvup_pc` のロジックは
  逐語維持です（改名と癖のコメントのみ）。
  `save_skl_type` の `char bs` も元のままです。

### 見つけた癖（すべて維持、コメントに記録）

- `skill_crt_check` はヒープ範囲外読みをします
  （潜在バグ、直していません）。武器種・素材種の
  添字が `id->type & WT_MASK`（最大 0xf0）/
  `& MT_MASK`（最大 0xf00）で、`WT_PLACE` /
  `MT_PLACE` で割っていませんが、配列は 16 要素
  しかありません。読み値は後段で割られます。
  `skillup_create` は正しく割っており、非対称です。
  式と確保方法を逐語維持し、s2 プローブで
  type 0x1234 のアイテムを使って動的にも
  踏みました（生成・コスト・表示が前後一致）。
- `PER_DICE` は評価のたびに振り直されます。
  `skillup` は 1 回の判定で 3 回評価するため、
  変数への巻き上げは厳禁です。
- `save_skl_type` の `char bs` は残りサイズの
  下位 8 ビットしか保持しません（256 → 0、
  128〜255 は負値 → `strlcpy` には巨大な
  `size_t`）。到達可能な範囲では無害で、逐語維持
  しました。
- 保存書式 `":%.0i"` は 0 を空文字で書きます
  （裸の `:`）。読込側は `:` の後の非数字を 0 と
  読むため往復が成立します。
- `put_skills` の `"#chr-D %-15s VX"` ヘッダは
  常に `cd[0].uid` を使い、`cd[n].uid` では
  ありません。本文は `buf+25` から書きます。
- `load_skills` は `load_skl_type` の非 0 戻り
  （1 = スキル未確保。`SKL_SPECIAL` は上限 0 で
  保存先が無いため `:Ssp` 行も常に該当。-1 =
  書式エラー）でも走査を打ち切らず、末尾まで
  読んで「no parameter」ログ + 戻り -1 に
  なります。エラー行も途中まで読み込まれます。
- `load_skl_type` の早期終了：`:` が無く
  `\n` / `\0` なら残りを 0 埋めして 0、他の
  文字なら -1。`:` の後が非数字なら進めずに
  その値を 0 とします。
- `status_skills` の引数解析：先行空白は読み
  飛ばし、単独の `s` も読み飛ばします
  （`skills` は全表示）。それ以外は戻り 1 で、
  裸の `skill` は発言として扱われます
  （`new_command.cpp` のフォールスルー仕様）。
- `status_skills` の整形：`k` は EF1/EF2/EF3 の
  ループをまたいで累積し（4 件ごとのフラッシュ）、
  `use_effect` のオフセット +11 / +46 / +61 は
  EF の範囲に対応します。FN 部は `i%3==0` で
  フラッシュするため最初の 1 件が単独行に
  なります。detect / create の小見出しは最初の
  フラッシュ時にだけ遅延表示されます。
- `skill_init`：全ての敵が 1 つの `npc_skill`
  配列（全値 5000）を共有します。
- `skillup_magic` の `m==0` は「相手なし」
  （詠唱のみ・空撃ち）の規約で、`check_lvup_pc`
  を飛ばし上限 10 です。`m>0` は PC 対 PC の
  制限を受けます。
- `skillup_magic_one` の `j = 100 / NCN_COAT(n)`
  は `NCN_COAT` が double のため浮動小数点除算
  してから int 化されます。`type >= SKL_MAGIC_FN`
  は短絡でダイスを振りません。SAVELIFE 分岐
  （術者死亡時：上限 99 / 確率 90 / 経験 20、
  それ以外：98 / 50 / 1）も元のままです。
- `skill_mg_calc` と `skillup_magic` の非対称：
  `EF_NONE` は前者が 0 を返しますが、後者は
  AT スキル 2 / 0 を振ります
  （`rgn=(0-1)%4+3=2`、`dmg=(0-1)/4=0`）。
  リテラルのオフセット 3 / 7 / 2 は
  `SKL_MGAT_LINE` / `ROOF` / `HEAL` です。
- `skillup`：`exp>=100 && lv>=limit` なら
  `exp=99`、それ以外は `lv*100+exp` の繰り
  上がりでレベルアップし、9999 で切り詰めます。
  `i=(int)(log10(exp+10.0)*10.0)`、
  戻り 0 は `n>=MAX_NTTYS` のときだけです。
- `skillup_weapon`：上限は `10+sp`、
  `WT_WIDE` / `WT_AROUND` は 50、
  `strlen(special_com[type][0])==sp/10` なら 99。
  反撃側のスキルアップは敵では無操作です
  （`skillup` の `m>=MAX_NTTYS` ガード）。
- `skill_down_dead`：-10 し、[90,100) は 100
  （習得した lv-1 は下がらない）、負は 0。
  `skill_down_idle`：`PER_DICE<lv` で -1、
  `==99` は 100、負は 0。
- `skillup_create` の非対称：`SKL_DETECT_MT`
  だけ確率 70 / 増分 `5+l` で、他は 50 / `2+l`。
- `skillup_magic` の振り分け：`ef<=EF_HEALS`
  または 40〜45 は攻撃/回復系、11〜39 は EF1、
  46〜60 は EF2、61〜93 は EF3。Pierce /
  Explode は Line も鍛えます。`ef>=EF_TRAPHP`
  または `chrg` は戻り 0。
- `check_lvup_pc`：`death_penalty` か、どちらかの
  `exp==99999999` で 0（`is_kill_master` の短絡が
  `cd[].exp` の読みを守ります）。同一 IP は
  `pc=2`、`(lv_up_between_pc&3)<=pc` で 0。

### 検証

- ビルド：`g++ -std=c++17` で `make` が通り、
  `-fcommon` は不要です。単一ファイルの警告は
  53 → 22 になりました（`-Wwrite-strings` が
  44 → 13。残りはリテラル接尾辞 1 件・符号比較
  1 件・ヘッダ由来の未使用変数 7 件で従来通り、
  新規の警告はありません）。
- 新規プローブ `probe_skills.py`（+ `run_skills.sh`
  / `compare_skills.py`）を作り、書き換え前
  2 回・後 1〜2 回実行しました。
  - base（スナップショットの phi.cd）：全スキル
    9999 の master と全 0 の guest の 2 セッション。
    正規化トランスクリプト 412 行・保存 `:S`
    14 行・phi.log 10 行が前後完全一致しました。
    （`[DM負荷: ...]` 行の数字はサーバ負荷の
    タイミングゆらぎで、書き換え前同士でも
    揺れるため比較時に潰しています。）
  - s1（端ケースの `:S` 行：空値・短い行・
    大きな値・`:Ssp`・書式エラー 2 種）：guest の
    読込と再保存を検証しました。トランスクリプト
    429 行・保存 `:S` 19 行・phi.log 13 行
    （`no parameter.(load_skills)` 3 行を含む）が
    前後完全一致し、保存内容も `%.0i` の 0 空文字
    化・末尾 0 の切り捨て・部分読込の通りに
    なっていることを確認しました。
  - s2（Create 呪文の実演：鑑定スキル 500/600/700
    と type 0x1234 の素材アイテム）：生成フロー・
    MP/GP コスト・スキル表示は前後一致しました。
    差分は書き換え前同士でも出る 3 種のゆらぎに
    限られます：生成物の系統名（Air / Light /
    Spirit claw と実行ごとに変化）、
    `skillup_create` のダイス成否による保存
    `:S` の差、接続スロット番号（C0001/C0002）と
    それに付随する行・phi.log の行数です。
- 回帰プローブ：
  - 単独セッション（`probe_single.py`）：差分は
    NPC の Cat（C0043）の徘徊フレームのみで、
    Cat を含む `#m57` フレームを除くと前後
    完全一致しました（書き換え前同士でも同じ
    種類の差が出ます）。
  - `#m57` プローブ：敵なし variant n は地図
    バイトとテキスト 115 行が前後完全一致
    しました。
  - アイテム使用（`probe_use.py`）：変種 a は
    全文 182 行と断定が一致し、phi.log の差は
    第7段階で記録済みの終了時系列のタイミング
    由来（`cli-err` 行と
    `data files are back up...` 行の順序）でした。
    変種 b は全文 119 行・構造値・phi.log 9 行
    まで全て一致しました。
  - 戦闘（`probe_calc.py`）：スクリプト応答は
    一致し、残る差は敵の徘徊・戦闘チャットと
    派生する HP / 経験値のみでした（第6段階と
    同じ切り分け）。`skill` 表示ブロックは一致し、
    ゲストの死亡（`skill_down_dead` 経路）も
    前後とも発生しました。
  - パーティ（`probe_party.py`）：要約 JSON が
    一致し、正規化 transcripts（A 78 行・
    B 79 行）も前後完全一致しました。
- 動的カバレッジの限界（逐語維持＋差分レビューで
  担保）：`skill_down_idle` の長時間放置による
  劣化（周期処理のため決定的に踏めない）、
  `skillup_weapon` の WT_WIDE/AROUND と
  special_com 上限 99 の分岐、`skillup_magic` の
  PC 対 PC 制限（m>0）と SAVELIFE の術者死亡時
  分岐、高位エフェクト呪文の実キャストによる
  EF2/EF3 スキルアップは、決定的なプローブが
  作れないため動的には未カバーです。
- 実サーバ 2 台（20017/20018）には触れていません。
  テストサーバ（20019）は各実行後に PID で
  停止済みです。

### 次のモジュール

`phi_attack.cpp` と進みます。`load_dd()` の
`memcpy` 重なりは `phi_data.cpp` の段階で
対応します。

## 第9段階：phi_attack.cpp（戦闘）

対象：`phi_attack.cpp`（書き換え前 1406 行 → 書き換え後 1452 行）。物理攻撃の
中核で、`attack_other()`（攻撃コマンドの入口：4方向の対象走査と武器種別
ごとの処理）、`attack_to()`（攻撃1回の解決：命中・ダメージ・劣化・反撃・
特殊攻撃・吸収）、`spat_check()`（com のコマンド列と登録パターンの照合）、
劣化計算の `degrade()` / `speed_degrade()`、`escape()`（NPC の逃走）、
`catch_monster()`（捕獲）などからなる。`attack_to()` は `phi_magic.cpp` /
`phi_use.cpp` からも呼ばれ、`degrade()` / `speed_degrade()` は
`phi_data.cpp` / `phi_dm.cpp` / `phi_magic.cpp` / `phi_use.cpp` からも
呼ばれる（宣言は `phi_attack.h` のまま不変）。

### 書き換え内容

- ヘッダを C++ 化：`<stdio.h>` `<stdlib.h>` `<string.h>` `<math.h>` →
  `<cstdio>` `<cstdlib>` `<cstring>` `<cmath>`、`<array>` を追加。
  未使用の `<ctype.h>` と、機種別に `<math.h>` を切り替える
  `#ifndef Mips` ガードを削除。
- 特殊攻撃名テーブル `special_at[16][13]` をファイル内限定の
  `constexpr std::array<std::array<const char *, 13>, 16>` に変更。
  行ごとの要素数の違い（0 行目は 11 個、残りは nullptr 埋め）もそのまま再現。
- ただし `special_com[16][13]` は `char *` のまま据え置き。
  `ext_skills.cpp` が `extern char *special_com[16][13]` として参照する
  モジュール間インターフェースのため、型を変えると宣言が食い違う。
  コード内コメントで明記。
- 劣化率テーブル `degrade_rate[16]` を `constexpr std::array<int, 16>` に変更。
- ファイル内限定だった `friend_attack` フラグを無名名前空間へ移動
  （理由は「見つけた癖」3 参照）。
- `attack_to()` の作業バッファ `char buf[256]` を `std::array<char, 256>`
  にし、利用箇所は `.data()` / `.size()` を渡す。`spat_check()` の
  `char buf[16]` も同様。
- NULL → nullptr（27 か所。コメント内の 1 件は旧コードの引用のため
  NULL のまま残置）。
- 標準関数の限定：`std::strlen`（3 か所）、`std::strncmp`（2 か所）、
  `std::floor`（1 か所）、`std::abs`（5 か所）。`strlcpy` は従来どおり
  無修飾（glibc/bsd 由来。第8段階と同じ扱い）。
- `check_write()` へ渡す文字列リテラル 4 件（`#attack\n` など）に
  `(char *)` キャストを付け、書き換え可能な文字列へのポインタを渡す
  旧来の形を明示。
- コメントや文字列を除いたトークン列の前後比較で、上記以外の差分が
  ないことを確認。警告は 477 件 → 267 件（`-Wwrite-strings` が
  475 件 → 265 件。残りの大半は `special_com` の文字列リテラル 208 件で、
  型がインターフェースのため据え置き）。

### 見つけた癖（すべて存置）

1. **`speed_degrade()` の `IT_FOOD` に `break` がなく `IT_JEWEL` へ
   フォールスルーする**。`degrade()` 側は 20010220 の修正で `break` が
   入ったが、こちらは直っていない。食べ物は宝石の処理（耐久 -1、
   ATP の 15/16 倍、VALUE の半減）まで受ける。
2. 反撃判定の `switch (i)` は字下げに反して `for` ループの外にある
   （ループ本体は defender の com から `'A'` を探す `if` 文だけ）。
   GCC の `-Wmisleading-indentation` が正確にここを指す。PC の defender は
   index 1 から、NPC は 0 から探し、`'A'` がなければ i == 10 で default。
3. `friend_attack` はファイル static の状態で、`attack_other()` が
   「正面セルにパーティメンバーがいるか」だけで決め、`attack_to()` が
   消費する。実際の攻撃方向とは無関係に正面だけを見る。`attack_to()` の
   呼び出しは `attack_other()` 経由に限られるため、値は常に直前に
   セットされたものが使われる。
4. 槍の2セル攻撃で `T_SPEARBARRIER` を判定するのは対象セルではなく
   手前セルのチップ（共有ポインタ `pmap` が手前を指したまま）。
5. `attack_to()` の結果変数 `i` は `WT_STINGER` のときアイテム探索
   ループに使い回され、後段の AE 判定 `if (!i && ...)` の意味が変わる。
   `AE_DRAIN` も `i` をレベルとして再利用する。
6. `spat_check()` の com コピーは NUL バイトを飛ばして詰める（終端で
   止まらない）。空パターン `""` は常時マッチし、その場合だけ dr が 0 の
   まま名前をコピーする（呼び出し側は dr == 0 を使わない）。照合は
   `j <= skill`（skill は装備武器のスキルから `fisqrt(skill/100 + 1)`）で
   制限されるため、低スキルでは長いパターンにマッチしない。
7. ファンブル時はアイテム探索に失敗しても（i == MAX_ITEMS）
   `degrade()` を呼ぶ。ダメージ後の武器劣化には、ファンブル側にある
   `save_id` の else 節がない（非対称）。
8. `attack_other()` の4方向ブロックは座標式だけが違う同型の繰り返し。
   統合せず形を維持（`phi_m57.cpp` の方針と同じ）。

### 検証


- 文字列・文字リテラルの多重集合が前後で完全一致（文字列 425 件・
  文字 1 件）。
- コメントと文字列を除いたトークン列の前後比較で、意図した書き換え
  以外の差分なし。
- 回帰プローブ（テストワールド、ポート 20019）：
  - `probe_single.py`：正規化後一致（差分は Cat の徘徊位置の
    `#m57 O` 行のみ）。
  - `probe_m57.py` 敵なし変種 n：地図バイト・テキスト 115 行とも完全一致。
  - `probe_party.py`：サマリ JSON・両トランスクリプトとも完全一致。
  - `probe_calc.py`：スクリプト応答部分は一致。残る差分は NPC 同士の
    戦闘チャットのみで、before 同士の差分（42 行）より before/after の
    差分（28 行）の方が小さい。
- 戦闘専用プローブを新設した（`~/workspace/phi-cpp-verify/probe_attack.py`）。
  地図（MapEvent）を見て最寄りの敵を追い、隣接したら向きを合わせて
  `hit` する追跡型で、`guard` と後退も混ぜる。装備データは
  `gen_cd_attack.py` が生成する `phi_cd_attack_a.txt`（guest:
  Sword/Hammer/Whip/毒剣/槍/Armor/Charm、master: Sword/Hammer/Fang）、
  実行は `run_attack.sh`、パターン比較は `compare_attack.py`。
  guest と master の2セッションを before×2 / after×2 実行：
  - スクリプト部分（ログイン〜装備〜序盤）は正規化後 243 行まで完全一致。
  - 以後は敵の徘徊で経路が分岐するため、メッセージのパターン種別と
    主要イベント回数で比較した。4ランとも攻撃行 8〜11 件（うち
    プレイヤー発 3〜4 件）・殺害 5〜6 件。カウンター・暴発・破損・
    クリティカル・痺れ・MP ダメージ等の低頻度イベントは before/after の
    区別なくランごとに散らばって出現し、種別の差はすべて「誰が誰を
    何で攻撃したか」の組み合わせと NPC の台詞の違いだった。
    プレイヤーによる殺害（Dog は Guest に殺された）も after ランで発生。

### 次のモジュール

`phi_enemy.cpp`（敵 AI、約2700行）。

- 戦闘まわりの検証には今回作った追跡型プローブ
  `~/workspace/phi-cpp-verify/probe_attack.py` が使える。地図を見て敵を
  追って殴るので、命中・反撃・劣化・殺害まで自動で踏める。装備データは
  `gen_cd_attack.py` が生成する `phi_cd_attack_a.txt`、実行は
  `run_attack.sh`、比較は `compare_attack.py`。
- `phi_attack.h` の宣言（`attack_other` / `attack_to` / `degrade` /
  `speed_degrade` / `spat_check`）は他モジュールから参照される。
  `phi_enemy.cpp` の書き換えで変えないこと。

## 第10段階：phi_enemy.cpp（敵 AI）

対象：`phi_enemy.cpp`（書き換え前 2733 行 → 書き換え後 2732 行）。敵 NPC の
思考ルーチン全体で、`auto_action()`（think 状態ごとの行動決定：会話・購入・
回復・逃走・追跡）、4種の歩行評価 `try_to_attackstep()` /
`try_to_wizardstep()` / `try_to_tracestep()` / `try_to_runstep()`（評価表を
作って最大値からサイコロで選ぶ。乱数と判定順序がそのまま敵の動きになる）、
`attack_check()` / `magic_check()`（武器・魔法の選択）、`angry_check()` /
`cooldown_check()`（怒りとクールダウン）などからなる。`phi_attack.h` の
共有宣言と `special_com` には触れていない。

### 書き換え内容

- ヘッダを C++ 化：`<stdio.h>` `<string.h>` `<ctype.h>` `<math.h>` →
  `<cstdio>` `<cstring>` `<cmath>`、機種別ガード付きの `<stdlib.h>` →
  `<cstdlib>`、`<time.h>` `<limits.h>` → `<ctime>` `<climits>`、`<array>`
  を追加。未使用の `<ctype.h>` を削除（`ftolower` はプロジェクト側の関数）。
- `phi_enemy.h` と重複していた `MC_*` / `STEP_*` の define（同値）を
  .cpp 側から削除し、ヘッダ側に一本化。
- ファイル内限定テーブルを無名名前空間の `constexpr std::array` 化：
  `go_com[7]` → `kGoCom`、`sx[25]` / `sy[25]` → `kSx` / `kSy`（視界 5x5 の
  走査順）。`input_analyze()` の引数が `char *`（`phi_dm.h` のモジュール間
  インターフェース）のため、`kGoCom` の利用箇所 4 件で `const_cast`。
- `use_charged_item_check()` の `char buf[40]` を `std::array<char, 40>`
  にし、利用箇所は `.data()` / `.size()` を渡す。一方
  `auto_action()` / `random_walk()` / `npc_talk()` の `buf` は
  `CPYSTR` / `CATSTR`（`sizeof(d)` を使うマクロ）で使うため C 配列のまま
  （コード内コメントで明記。第4・第8段階と同じ理由）。
- NULL → nullptr（33 か所、`time(nullptr)` の 6 件を含む）。
- 標準関数の限定：`std::snprintf`（33）、`std::memcpy`（3）、
  `std::strlen`（1）、`std::strchr`（1）、`std::abs`（6）、
  `std::log`（3、`accept_master()` の対数によるレベル比較）。
- `input_analyze()` / `str_search()` へ渡す文字列リテラルと `NTK_*`
  マクロに `(char *)` キャスト（32 件。このファイルでは三項演算子の
  `NTK_*` に元々 `(char*)` キャストがあり、その流儀に合わせた）。
  `dmprintf()` の書式マクロ `DMM_DESTRUCT_MONEY` / `DMM_DESTRUCT` は
  第7段階（phi_use）と同様に無修飾のまま。
- 警告は単体で 54 件 → 15 件（残りは dmprintf 書式マクロ由来の
  write-strings 2 件、符号混じり比較 6 件、字下げ 4 件、代入のみの
  変数 2 件、`phi_dm.h` 由来の literal-suffix 1 件）。全体では
  1509 件 → 1470 件。

### 見つけた癖（すべて存置）

1. **`attack_enemy()` の魔法選択条件が `cd[n].wd == TH_NONE`**。
   `wd`（武器の攻撃方向マスク）と比べる定数が think 用の `TH_NONE`。
   `TH_NONE` も `WD_NONE` も 0 なので同値で動くが、`attack_angry()` 側は
   `WD_NONE` を使っており非対称。コード内コメント付き。
2. **`use_charged_item_check()` の未設定判定は `!ff`**。
   `magic_check()` の `ff == -1` と非対称で、添字 0 の候補が選ばれると
   未設定扱いのままになり、後続の候補が PER_DICE なしで上書きする。
   コード内コメント付き。
3. `is_effective_effect()` の `EF_HEALS` は `(f & 0x40)` で判定する。
   0x40 は `ED_*` の名前付きフラグのどれでもないビットで、
   `direction_check()` は立てない。`ED_NORESTRICT`（0x7f）を渡したとき
   だけ立つため、実質「制限なし」の判定になっている。
4. PER_DICE は評価のたびに振り直される（第8段階と同じ性質）。
   `angry_check()` の召喚獣の早期リターン条件は moral / align /
   cooldown の 3 回を別々に振り、`accept_master()` は三項演算子の
   枝ごとに振って `log()` の浮動小数点で比べる。
5. `random_walk()` の `pdx` / `pdy` は代入のみで未使用
   （`-Wunused-but-set-variable` が指摘）。「go で進めたか」の判定は
   `xx` / `yy` だけを見る。
6. 符号混じり比較 6 件：`cd[n].id[i].active_time <= time(nullptr)`。
   `active_time` は unsigned long（番兵は `ULONG_MAX`）、`time()` は
   time_t（signed long）で、比較では time_t 側が unsigned 化される。
   現実の時刻は正なので実害はない。形を維持。
7. 字下げと制御構造の不一致 4 件（`-Wmisleading-indentation` が指摘）：
   `run_walk()` の二重ループ内の continue / return、`auto_action()` の
   CT_ERASER の塊、`attack_enemy()` の魔法探索ループ、
   `temptation_to_attack()` のループ。また `attack_check()` 冒頭の
   FF_ATTACK チェックは、字下げでは `enemy_check` の内側に見えるが
   独立した文（無条件の早期リターン）。
8. `enemy_check()` の switch は `CT_NORMAL` の固有チェックの後に
   break がなく、`CT_BEAST` の同名チェックへフォールスルーする
   （NORMAL にも同種判定を効かせる意図と見える）。`move_check()` の
   switch は `default` が `case CT_ASTRAL` より前に置かれている。
9. `auto_action()` の購入処理（food / weapon / armor / accessory /
   charge）は、空きスロット探索で得た添字 `i` を、get でそのスロットに
   入ったアイテムの添字として使い回し、支払額もその前提で計算する。
10. `DEBUG_ANGRY` のログが使う `debug_buf` の定義は `#ifdef DEBUG` の
    ときだけ（第6段階の `phi_calc.cpp` と同じ構造）。`-DDEBUG_ANGRY`
    単独では前後とも同じ 3 か所でコンパイルエラーになる。

### 検証

- 文字列・文字リテラルの多重集合が前後で完全一致（文字列 88 件・
  文字 6 件）。
- プリプロセッサ行とコメントを除いたトークン列に、意図した変換の
  逆変換（`std::` 限定の除去、改名の復元、nullptr → NULL、キャストの
  除去、`std::array` 宣言と `buf.data()` / `.size()` の復元、
  namespace の除去）を適用すると前後完全一致（24,032 トークン）。
- `-DDEBUG -DDEBUG_ANGRY` ビルドが通ること、`-DDEBUG_ANGRY` 単独では
  前後とも同じエラー（`debug_buf` 未定義 3 件）になることを確認。
- 回帰プローブ（テストワールド、ポート 20019。attack のみ
  before×2 / after×2、他は各 1 回）：
  - `probe_m57.py` 敵なし変種 n：地図バイト・テキスト 115 行とも
    完全一致。
  - `probe_single.py`：正規化後一致（差分は Cat の徘徊位置の
    `#m57 O` 行 1 件のみ）。
  - `probe_party.py`：サマリ JSON・両トランスクリプトとも完全一致。
  - `probe_calc.py`：スクリプト応答は種別・件数とも一致。差分は
    NPC 同士の戦闘チャットと台詞、数値の桁変わりによる表示位置の
    違いのみ。
  - 戦闘プローブ `probe_attack.py`（追跡型）：スクリプト部分の共通
    接頭辞は before/after 間で 237 行（before 同士の 219 行より長い）。
    以後は敵の徘徊で経路が分岐するため種別と回数で比較した。
    4 ランとも殺害 6〜7 件・痺れ 2〜4 件・NPC 台詞 4〜10 件で、
    詠唱・回復は before / after の両側に出現し、プレイヤーによる
    殺害（Dog は Guest に殺された）も after の 2 ランとも発生した。
    種別の差はすべて「誰が誰を何で攻撃したか」の組み合わせ違い。

### 次のモジュール

`phi_magic.cpp`（魔法、5768 行）。

- `mgstrlen()` の宣言（`phi_magic.h`）は `phi_enemy.cpp` が多用する
  インターフェース。`phi_magic.cpp` の書き換えで変えないこと。
  同じく `phi_enemy.h` の宣言（`select_magic` / `magic_check` 等）は
  `phi_magic.cpp` からも呼ばれる。
- 敵 AI が絡む検証には追跡型プローブ
  `~/workspace/phi-cpp-verify/probe_attack.py` が使える。装備データは
  `gen_cd_attack.py` が生成する `phi_cd_attack_a.txt`、実行は
  `run_attack.sh`、比較は `compare_attack.py` と種別カウント。

## 第11段階：phi_magic.cpp（魔法）

対象：`phi_magic.cpp`（書き換え前 5768 行 → 書き換え後 5779 行）。呪文の
解析と実行全体で、`spell_analyze()` が呪文名から各実装へ振り分け、
回復・攻撃（line / sphere / area）・探知・生成・チャージ・各種リスト
表示などを担う。申し送りどおり `phi_magic.h` の `mgstrlen()` 宣言、
`phi_enemy.h` の宣言（`select_magic` / `magic_check` 等）、
`phi_attack.h` の共有宣言と `special_com` には触れていない。

### 書き換え内容

- ヘッダを C++ 化：`<stdio.h>` → `<cstdio>`、`<string.h>` →
  `<cstring>`、`<signal.h>`（重複あり）→ `<csignal>`、`<math.h>` →
  `<cmath>`、`<time.h>` → `<ctime>`、機種別ガード付きの `<stdlib.h>`
  → `<cstdlib>`、`<limits.h>` → `<climits>`、`<array>` を追加。
  未使用の `<ctype.h>`（ctype 関数は未使用）、`<memory.h>`
  （`memcpy` は `<cstring>` で足りる）、`<sys/time.h>`（timeval 系は
  未使用）、`<sys/types.h>` を削除。`phi_attack.h` の重複 include
  （2 回現れた）も 1 回に整理。
- ファイル内限定テーブルを無名名前空間の `constexpr std::array` 化：
  `create_item()` の素材係数 `mt_atp[16]` / `mt_dfp[2][16]` /
  `mt_mrp[2][16]` / `mt_er[16]` → `kMtAtp` / `kMtDfp` / `kMtMrp` /
  `kMtEr`（すべて読み取り専用）。2 次元テーブルは `std::array` の
  集約初期化に合わせ、初期化ブロック全体をもう一段波括弧で包んだ
  （行・値は不変。元の `{行},{行}` 形式のままだと「too many
  initializers」になる）。shutdown の隠し呪文文字列 `cthulhu` も
  無名名前空間の `constexpr char kCthulhu[]` にした。
  `friend_magic`（パーティ攻撃用フラグ）は第9段階の `friend_attack`
  と同じく無名名前空間へ移した。
- バッファ 24 個を `std::array<char, N>` 化し、利用箇所は
  `.data()` / `.size()` を渡す：`user_lists` / `master_lists` /
  `wizard_light` の `buf` + `sp`、`wizard_eye_f` の `buf` / `nbuf` /
  `sp1` / `sp2` / `psbuf`、`create_item` / `detect_fn` /
  `detect_magic` / `effect_analyze` / `mg_heal` 系 5 つ /
  `mg_erase` / `mg_encourage` / `mg_effect` / `mg_line_attack` /
  `mg_sphere_attack` / `mg_area_attack` / `mg_attack_to` /
  `list_inv` / `mg_save_life` / `charge_spell` の `buf`。
  1 行に複数書かれていた宣言（`buf,sp` や `sp1,sp2,psbuf`）は
  1 行 1 変数に分割した。`mg_effect` の `buf[0]` 参照・代入は
  `operator[]` のまま成立する。
- C 配列のまま残したバッファ（理由をコード内コメントで明記）：
  `spell_analyze` の `buf`（CPYSTR / CATSTR で使う上、`buf + 2`
  などのポインタ演算と末尾の書換えでも使う）、`create_item` の
  `name`、`detect_item` の `buf`、`ident_to` の `buf`。いずれも
  CPYSTR / CATSTR（`sizeof(d)` を使うマクロ）で使うため。
  `debug_buf`（`#ifdef DEBUG` のグローバル）は従来どおり。
- NULL → nullptr（80 か所、`time(nullptr)` の 5 件を含む。`time`
  自体は第10段階と同様に無修飾のまま）。
- 標準関数の限定：`std::snprintf`（170）、`std::strcmp`（59）、
  `std::strncmp`（16）、`std::strlen`（4）、`std::memcpy`（2）、
  `std::abs`（35。`frand` は整数マクロで、他の引数も座標差などの
  整数）、`std::atoi`（9）、`std::log10`（17）。`fprintf` /
  `fopen` / `fclose` と `strlcpy` / `strlcat` は無修飾のまま。
- `dmprintf()` / `dmcheck_write()` へ渡す DMM_* 書式マクロは
  第7段階と同様に無修飾（`(char *)` キャストなし）。`check_write()`
  の文字列リテラル 59 件も従来どおり無修飾。
- 警告は単体で 391 件 → 391 件（内訳まで完全一致：write-strings
  379 件、符号混じり比較 4 件、未使用変数 `j` 2 件、
  format-security 2 件、字下げ 2 件、代入のみの変数 `d` 1 件、
  `phi_dm.h` 由来の literal-suffix 1 件）。全体でも 1470 件 →
  1470 件。write-strings の大半は DMM_* マクロ経由であり、
  この段階では減らさない方針のため不変。

### 見つけた癖（すべて存置）

1. **`mg_attack_to()` の `FN_PRISM` ケースに break が無く、
   `default` へ落ちる**（コード内コメント付き）。`tfn = dfn + 1`
   （上限なら `FN_EARTH`）の計算結果は、`default` 側の
   `tfn = m->fn` で上書きされて捨てられる。潜在バグ。
2. **`charge_spell()` の extension 連結が `cd[n].md[n]` を読む**
   （コード内コメント付き）。添字はキャラ番号 `n` で、本来は
   呪文番号 `m` と思われる。他人の呪文の extension が混ざる。
   潜在バグ。
3. 「Book of the Yog=Sothoth」の保存で `fprintf(fp, buf)` の
   書式なし出力が 2 件（format-security 警告）。`buf` は
   `save_item()` が生成した自前文字列。
4. 符号混じり比較 4 件（`ip_ban` の時刻比較 2 件、`active_time`
   2 件）。第10段階の癖 6 と同種。
5. `user_lists` / `master_lists` の変数 `j` は宣言のみで未使用。
   `mg_heal` の `d` は `d = cd[n].d;` の代入のみで使われない。
6. 字下げが誤解を招く箇所 2 件（`mg_heal_friend_one` の `for`、
   `display_single_magic` の `if`）。制御構造は字下げと違う。
7. `Appear` の ebuf / ebuf2 シャッフルで、コメント「存在している
   シャッフル」「存在していないシャッフル」と中身が逆になっている。
8. `Disappear` はループ変数 `i` にコントローラ添字
   （`cd[j].vf2`）を代入して使い回し、ループ後の `i == 0` で
   出すメッセージを選ぶ。
9. 「Book of the Yog=Sothoth」の `buf[strlen(buf)-1] = '\0'` は、
   `save_item()` が空文字列を返した場合に `buf[-1]` へ書く。
   現在の呼び出し経路では起きていない。
10. `detect_fn()` の引数 `fmt`（`phi_magic.h` の `char *`
    インターフェース）に文字列リテラルを渡す呼び出しがあり、
    write-strings 警告の一因になっている。インターフェース側の
    話として存置。

### 検証

- 文字列・文字リテラルの多重集合が前後で完全一致（556 件）。
- プリプロセッサ行とコメントを除いたトークン列に、意図した変換の
  逆変換（`std::` 限定の除去、改名の復元、nullptr → NULL、
  `std::array` 宣言と `.data()` / `.size()` の復元、namespace の
  除去、2 次元テーブルの波括弧の復元、複合宣言の分割の復元）を
  適用すると前後完全一致（53,883 トークン）。
- バイナリの `strings` 比較（8 文字以上）：差分は無名名前空間化に
  伴うシンボル名（`_ZL...` → `_ZN12_GLOBAL__N_1...`）と配置差
  のみで、メッセージ文字列は不変。
- 回帰プローブ（テストワールド、ポート 20019）：
  - `probe_m57.py` 敵なし変種 n：地図バイト・テキスト 115 行とも
    完全一致（master の `cast` → Eagle eye を含む）。
  - `probe_single.py`：正規化後一致（`spells` 表示・各種 master
    コマンドを含む）。
  - `probe_party.py`：サマリ JSON はバイト一致、両トランスクリプト
    は正規化後一致。
  - `probe_calc.py`：書き換え前同士の差分行数（418）が前後差分
    （373）より大きく、差分は NPC 同士の戦闘・詠唱のサイコロ
    結果のみ。`cast` の応答も両側で確認。
  - `probe_skills.py` base / s1 / s2：s1 はトランスクリプトが
    全ラン一致。phi.cd の `:S` 行・レコードの順序と phi.log の
    cli-err 行は接続スロット確定のタイミングで揺れ、同バイナリ
    同士でも反転することを確認した（base の差分は書き換え前
    同士で同一差分を再現。s1 は before1 と after2 の組で
    完全一致）。s2 の生成アイテム名（Fire claw / Light claw）の
    違いは `create_item` のサイコロによる fn 選択。
  - `probe_use.py` a / b / c1〜c5 / d / e / disc の 10 変種：
    すべて正規化後全文一致＋断定 OK（チャージアイテムの使用など
    魔法経路を含む）。
  - `probe_attack.py`（before×2 / after×2）：パターン件数比較で
    同バイナリ同士の対照と区別がつかない。NPC の詠唱（Cure /
    Harm）と回復・攻撃の解決は before / after の両側に出現した。
- 動的に踏めていない経路：shutdown の隠し呪文（`kCthulhu` 照合）
  や Book 系の保存など、プローブの脚本に無い呪文はトークン照合と
  書式・引数の照合で確認した。`MAX_OBJECTS == 0` や `NO_SKILL`
  定義時のビルドは従来どおり未確認だが、プリプロセッサの構造は
  そのまま保ってある。

### 次のモジュール

`phi_dm.cpp`（メインループ・コマンド振り分け、約5100 行）。
その次が最終の `phi_data.cpp`。

- `phi_magic.h` の宣言は今回も変更していない（`mgstrlen()` 等）。
  `phi_dm.cpp` からの呼び出しインターフェースはそのまま使える。
- 今回記録した潜在バグ 2 件（`mg_attack_to` の FN_PRISM
  フォールスルー、`charge_spell` の `md[n]`）は `phi_magic.cpp`
  内の話で、修正するかどうかは従来分（`OP_REVERSE` のクラッシュ、
  `skill_crt_check` の範囲外読み、`speed_degrade` の
  フォールスルー）と合わせてユーザー判断待ち。
- 検証ハーネスは今回のものが流用できる。なお `run_attack.sh`
  には実行権限が無いため `sh run_attack.sh ...` で実行すること。

## 第12段階：phi_dm.cpp（サーバ中核）

対象：`phi_dm.cpp`（書き換え前 5119 行 → 書き換え後 5140 行）。
メインループ、コマンド解釈（`command_analyze` / `input_analyze` /
`func_analyze`）、接続管理（`close_connect` /
`connection_error_process`）、地図・状態表示、メール、リスト表示
などを担うサーバ中核。申し送りどおり `phi_magic.h` / `phi_enemy.h`
/ `phi_attack.h` の宣言、`special_com`、`mgstrlen()` には触れて
いない。`px[4]` / `py[4]` は他モジュールが `extern int px[]` で
参照し `phi_dm.h` のマクロも使う共有インターフェースのため、
C 配列のまま残した。

### 書き換え内容

- ヘッダを C++ 化：`<stdio.h>` → `<cstdio>`、`<string.h>` →
  `<cstring>`、`<signal.h>` → `<csignal>`、`<time.h>` → `<ctime>`、
  機種別ガード付きの `<stdlib.h>` → `<cstdlib>`、`<errno.h>` →
  `<cerrno>`、`<limits.h>` → `<climits>`、`<array>` を追加。
  未使用の `<ctype.h>`（ctype 関数は未使用）、`<math.h>`
  （数学関数は未使用）、`<sys/time.h>`、`<sys/types.h>` を削除。
  `<unistd.h>`、AIX の `<sys/select.h>`、Cygwin の `<process.h>`、
  Mips の `<sys/timeb.h>` は従来どおり残した。
- ファイル内限定テーブルを無名名前空間の `constexpr std::array`
  化：コマンド表 `parsecom` → `kParsecom`
  （`std::array<const char*, 10>`、元の位置に配置）。
  `print_map` の関数内 `static char dc[4]` / `rdc[4]` もその場で
  `static constexpr std::array<char, 4>` の `kDc` / `kRdc` にした。
- バッファ 28 個を `std::array<char, N>` 化し、利用箇所は
  `.data()` / `.size()` を渡す：`main` の `errmsg` / `nm`、
  `print_status` / `print_lists` / `set_chr` / `auto_equip` /
  `talk_analyze` / `connection_error_process` / `charagra_set` /
  `next_level_check` / `level_up_print` / `delete_mail` /
  `check_mail` の `buf`、`func_analyze` の `wbuf`、
  `input_analyze` の `wbuf` / `wbuf2` / `fid`、`command_analyze`
  の `bufa` / `bufb` / `crgr` / `homeland`、`adduser` の `wbuf`、
  `errnum_to_msg` の `static msg`（戻り値は `msg.data()`）、
  `connection_error_process` の `fid`、`save_mail` の `buf` /
  `nbuf` / `sp`、`read_mail` の `rbuf` / `wbuf`、`readhomeland`
  の `buf` / `wid`（`p = buf.data()` の形に書き換え）。
  1 行に複数書かれていた宣言は 1 行 1 変数に分割した。
- C 配列のまま残したバッファ（理由をコード内コメントで明記）：
  `main` の `buf[1024]`（CPYSTR の宛先かつポインタ演算）、
  `signal_trap` の `buf` / `buf2`（CPYSTR の宛先）、
  `check_write` の `buf`（CPYSTR の宛先）、`print_map` の `buf`
  （`buf + 18` や `*(buf + 118)` のポインタ演算）と
  `last_npc` / `last_board_title`（CPYSTR の宛先・sizeof 依存）、
  `print_status` の `last_status` / `last_cond`（CPYSTR の宛先）、
  `itemlist` の `buf2`（CATSTR の宛先）、`command_analyze` の
  `buf`（"use" 分岐で CATSTR の宛先）、`new_user` の `wbuf`
  （CPYSTR の宛先）、グローバル `pre_adduser_ip`（CPYSTR の
  宛先）、`px` / `py`（共有インターフェース）。`log_print` の
  `tstr` は `#ifdef SPLITLOG` 内（既定構成では未コンパイル）
  のため手付かず。
- NULL → nullptr（119 か所、`time(nullptr)` を含む。`time`
  自体は従来どおり無修飾）。
- 標準関数の限定：`std::snprintf`（122）、`std::strcmp`（63）、
  `std::strncmp`（38）、`std::strlen`（24）、`std::strncpy`（2）、
  `std::memcpy`（3）、`std::memcmp`（1）、`std::atoi`（9）、
  `std::abs`（2。引数はいずれも整数）、`std::strchr`（3）、
  `std::strstr`（3）。`sscanf` は第11段階の前例どおり無修飾。
  `strlcpy` / `strlcat`、`time` / `localtime` / `asctime` /
  `strftime`、`signal`、`fopen` / `fclose` / `fprintf` /
  `fputs`、`exit` / `free` / `system`、`rename` / `unlink` も
  無修飾のまま。
- 警告は単体で 438 件 → 428 件。write-strings が 421 件 → 411 件
  （`kParsecom` のリテラル 10 件が `const char*` になり消滅）
  で、それ以外の内訳は完全一致（符号混じり比較 5 件、未使用変数
  3 件、parentheses 2 件、字下げ 2 件、format-truncation 2 件、
  unused-result 1 件、代入のみの変数 1 件、`phi_dm.h` 由来の
  literal-suffix 1 件）。

### 見つけた癖（すべて存置）

1. **`input_analyze` の `ST_BAN_` 処理は
   `dmprintf(n, 0, rbuf, sizeof(rbuf), ...)` と書かれており、
   `rbuf` は `char *` 引数なので `sizeof` はポインタサイズ（8）
   になる**（コード内コメント付き）。書式化した返信は入力
   バッファ `rbuf` の先頭 8 バイトに切り詰めて書かれ、それが
   `check_write` で送られる。潜在バグ。
2. `main` の `#chr-D` 処理で `get_cd` の戻り値 `case -2:` /
   `case -1:` に break が無く、`case 1:` の `continue` へ
   落ちる。接続を閉じた後に switch 下の成功パスへ進まない
   ための、荷重のかかったフォールスルー（コード内コメント
   付き）。
3. 意図的なフォールスルーは他にも多い：`move_chr` の
   `T_FLOWER` → `T_PLANT`、`connection_error_process` の
   `ST_WRITE` → `ST_RECEIVECOMPLETE` / `default`、`adduser` の
   `ST_USERAD` → `ST_USERGRP` → `ST_USERCF`、`input_analyze`
   の `ST_R_GET` → `ST_GET` / `ST_R_PUT` → `ST_PUT`、
   `command_analyze` の "go" の `'N'` → `'n'` など。
4. `check_write` の `buf[(i = strlen(buf) - 1)]` は、`mes` が
   空文字列だと `buf[-1]` を読む。第11段階の癖 9 と同種。
   現在の呼び出し経路で空が渡る例は見当たらない。潜在。
5. `twobyte_check` は空文字列だと戻り値 `m` が未初期化になる。
   実経路では `sscanf` の `%s` 経由のため通常は到達しない。
   潜在。
6. 符号混じり比較 5 件（`main` の `active_time < time(NULL)`、
   `ST_BAN` の `ip_ban` 時刻、`itemlist`、`command_analyze`
   2 件）。
7. 未使用変数：`command_analyze` の `buf2[20]`（コメント内で
   のみ参照される死んだ宣言）、`save_mail` の `i`、
   `delete_mail` の `i`。代入のみ：`input_analyze` の `l`。
   字下げが誤解を招く箇所 2 件（`talk_analyze` の `else`、
   `strindex` の `for`）。parentheses 警告 2 件は
   `twobyte_check` の `&&` / `||`。`signal_trap` の `system()`
   戻り値未使用。format-truncation 2 件（`read_mail` と
   `dm_msg_jp.h` のマクロ由来 1 件）。
8. `errnum_to_msg` のリテラルに "besouse" のタイポがある。
   リテラル一致の原則どおり存置。

### 検証

- 文字列・文字リテラルの多重集合が前後で完全一致（667 件、
  うち文字リテラル 262 件）。
- プリプロセッサ行とコメントを除いたトークン列に、意図した
  変換の逆変換（`std::array` 宣言 → C 宣言、複合宣言の分割の
  再結合、`.data()` の除去、`.size()` → sizeof、`std::` 限定の
  除去、nullptr → NULL、`kParsecom` → `parsecom`、`kDc` /
  `kRdc` の復元、`wbuf[0]` ↔ `*wbuf` の特殊形）を適用すると
  前後完全一致（40,618 トークン）。git の差分も目視で確認し、
  意図した変更区分以外は無い。
- バイナリの `strings` 比較：差分はコード生成に伴うバイト列の
  ノイズとシンボル名・配置差（`parsecom` の無名名前空間化、
  ジャンプテーブル番号）のみで、メッセージ文字列は不変。
- 回帰プローブ（テストワールド、ポート 20019）：
  - `probe_m57.py` 敵なし変種 n：地図バイト・テキスト 115 行
    とも完全一致。
  - `probe_single.py`：スクリプト部分は一致。差分は Cat の
    徘徊による地図フレーム 1 回分の有無のみ（第9〜11段階と
    同じ非回帰クラス）。
  - `probe_party.py`：サマリ JSON はバイト一致、両トランス
    クリプトは正規化後一致。
  - `probe_use.py` a / b / c1〜c5 / d / e / disc の 10 変種：
    全変種でトランスクリプト全文一致＋断定 OK。phi.log は
    8 変種で一致。a / c5 では終了時の順序差（切断処理が
    SIGTERM より先に進むと cli-err 行が出て、`lcnt == -1` の
    ため `signal_trap` のバックアップ行が出ない）が出たが、
    CPU 負荷をかけると書き換え前バイナリでも同一パターンを
    再現した。ハーネスがプローブ終了直後に kill する競合で、
    第7・11段階で記録した同種のタイミング差。
  - `probe_skills.py` base / s1 / s2：s1 はトランスクリプト・
    保存・phi.log すべて一致。base はトランスクリプト一致、
    保存 `:S` 行は集合として一致（`:SmgF` の位置差は保存
    タイミングの揺れで、第11段階で書き換え前同士の再現を
    確認済みのクラス）、phi.log 差分は上記の終了時競合
    （こちらは逆方向）。s2 は生成アイテム名 1 件
    （Spirit claw / Air claw）のサイコロ差と、それに伴う
    保存差のみ（第8段階と同じ）。
  - `probe_calc.py`：スクリプト行は一致。差分は敵の徘徊・
    戦闘ノイズと、Cure の暴発 1 回のサイコロ差のみ。
  - `probe_attack.py`（before×2 / after×2）：冒頭 46 行の
    スクリプト部分は 4 ランとも完全一致（分岐点はセッション
    ごとの地図シード）。パターン件数のばらつきは書き換え前
    同士の対照と同等で、after の 1 ランに武器落下
    （`phi_attack` のファンブル）1 回を含む。
- 動的に踏めていない経路：メール系の一部、世界移動
  （`#ch-srv`）、ユーザー追加系などプローブの脚本に無い
  経路は、トークン照合と書式・引数の照合で確認した。
  `SPLITLOG` / `NOUSERADD` 等の代替ビルドは従来どおり
  未確認だが、プリプロセッサの構造はそのまま保ってある。

### 次のモジュール

`phi_data.cpp`（データ入出力、約9900 行）が最終段階。

- **`load_dd()` の memcpy 重なり（ASan 指摘）は `phi_data.cpp`
  の担当**で、この段階では未着手。`phi_data.h` の宣言
  （`id2f` / `loadstr` / `server_version_check` 等）と
  `phi_dm.h` の extern 群は不変更のまま使える。
- `lcnt`（`phi_data.cpp` 側：初期値 -1、データロード中は
  0 以上）は `signal_trap` のバックアップ分岐の条件になって
  いる。切断保存が済んだ後の終了ではバックアップ行が出ない
  のは現行挙動として確認済み。
- 今回記録した潜在バグ 3 件（`ST_BAN_` の sizeof ミス、
  `check_write` の `buf[-1]`、`twobyte_check` の未初期化）は
  `phi_dm.cpp` 内の話で、修正するかどうかは従来分
  （`OP_REVERSE` のクラッシュ、`skill_crt_check` の範囲外
  読み、`speed_degrade` のフォールスルー、`mg_attack_to` の
  FN_PRISM フォールスルー、`charge_spell` の `md[n]`）と
  合わせてユーザー判断待ち。
- 検証ハーネスは今回のものが流用できる。なお `run_attack.sh`
  には実行権限が無いため `sh run_attack.sh ...` で実行する
  こと。

## 第13段階：phi_data.cpp（データ入出力、最終段階）

対象：`phi_data.cpp`（書き換え前 9876 行 → 書き換え後 9907 行）。
データファイルの読み書き・保存まわり（`load_dd` / `load_cd` /
`save_cd`、`phi.cd` / `phi.dd` / `phi.sd` 等のロードとセーブ、
バックアップ、ゲート・世界移動データ、ストア、タイトル、
ID バン、サークルのファイル保存など）を担う最大のモジュール。
申し送りどおり `phi_data.h` の宣言（`efopen` / `efgets` /
`efclose` 等）、`phi_dm.h` の extern 群、`phi_magic.h` /
`phi_enemy.h` / `phi_attack.h` の宣言、`special_com`、
`mgstrlen()`、`px` / `py` には触れていない。

### 書き換え内容

- ヘッダを C++ 化：`<stdio.h>` → `<cstdio>`、`<string.h>` →
  `<cstring>`、`<ctype.h>` → `<cctype>`、`<signal.h>` →
  `<csignal>`、`<time.h>` → `<ctime>`、`<stdlib.h>` →
  `<cstdlib>`、`<limits.h>` → `<climits>`、`<array>` を追加。
  未使用の `<math.h>`（数学関数は未使用）、`<memory.h>`、
  `<sys/time.h>`、`<sys/types.h>` を削除。`<unistd.h>`、
  `<dirent.h>` は従来どおり残した。
- ファイル内限定テーブルを無名名前空間の `constexpr std::array`
  化：拡張子表 `fnc[10]` → `kFnc`（`std::array<char, 10>`）。
- バッファ 118 個（宣言文 98 か所）を `std::array<char, N>` 化
  し、利用箇所は `.data()` / `.size()` を渡す。元のコードで
  同サイズの複数宣言が 1 行に書かれていた箇所は、変換後も
  1 行にまとめた形を保った。`load_fd` の `buf2` は
  `#ifndef AREA_SUB` で 2 つのサイズ（`MAX_X * 2 + 2` /
  `MAX_X * 3 + 2`）を持ち、両方とも式のまま変換した。
- C 配列のまま残したバッファ（理由をコード内コメントで
  明記、15 か所）：`save_cd` / `backup_cd` / `save_sav` の
  `buf`、`get_gate` の `buf`、`load_td_swt` の `buf2`、
  `load_switch` の `wbuf`、`load_gate` の `land`、`save_sd`
  の `rbuf` / `wbuf`、`set_title` の `rbuf`、`save_store` の
  `buf2`、`save_obj` の `buf`（CATSTR の宛先）、`load_sd` の
  `tstr`、`savef_id_in_circle` の `buf`、ファイルスコープの
  `pfname[128]`。いずれも CPYSTR / CATSTR の宛先で
  `sizeof` 依存がある。`cfile` / `savfile` は `efopen(char *,
  char *)`（`phi_data.h` の共有宣言）に渡すため `static
  char *` のまま（コメント明記）。グローバル `debug_buf[256]`
  は他モジュールにも同一の定義がある DEBUG 用のため不変更。
- NULL → nullptr（462 か所）。
- 標準関数の限定：`std::snprintf`（182）、`std::strcmp`（17）、
  `std::strncmp`（43）、`std::strlen`（17）、`std::memcpy`
  （36）、`std::memset`（6）、`std::memcmp`（1）、`std::atoi`
  （6）、`std::abs`（1）、`std::strstr`（2）、`std::isspace`
  （1）。`sscanf`、`strdup`（POSIX）、`time` / `localtime` /
  `strftime` / `asctime`、`fopen` / `fclose` / `fprintf` /
  `fputs` / `fgets`、`exit` / `free` / `malloc`、`rename` /
  `unlink` は従来どおり無修飾。
- 警告は単体で 232 件 → 231 件。差分は `load_dd` の
  `-Wrestrict`（memcpy 重なり、次節）1 件が消えたのみで、
  それ以外の内訳は完全一致（write-strings 199 件、符号混じり
  比較 6 件、字下げ 6 件、format-truncation 9 件、未使用
  変数ほか）。

### `load_dd()` の memcpy 重なりへの判断（第5段階からの申し送り）

対象は書き換え前 4442〜4443 行目の

```c
*yname = 0;
memcpy(yname + 1, yname, 31);
```

（`yname` は `phi_dm.h` で宣言された `char yname[32]`、暦号）
で、第5段階で ASan が `memcpy-param-overlap` を指摘した件。

**判断：`std::memmove` に変更した**（重なりを許す定義済みの
動作にする）。根拠は次のとおりで、結果のバイト列は変わら
ない。

1. `load_dd()` は起動時の初期化で 1 回だけ呼ばれ
   （`phi_dm.cpp`）、その時点で `yname` は BSS で全ゼロ。
   `*yname = 0;` の直後なのでコピー元 31 バイトはすべて 0
   であり、memcpy でも memmove でも結果は全ゼロでバイト
   単位で同一になる。
2. `yname` の読みは常に C 文字列として（`*yname` の判定、
   `%s` での表示）行われ、続く `loadstr(yname, &p, '\n',
   32)` が先頭から終端まで書き直すため、最初の NUL より
   後ろのバイトは観測されない。
3. 同じ「1 バイトずらし」コピーには同一コードベース内に
   memmove の使用例がある（`id_ban`）。

検証：書き換え前の ASan ビルドは起動時に
`memcpy-param-overlap`（`load_dd()`、4443 行目）で異常終了
することを再現した。書き換え後の ASan ビルドは異常終了
せずポート 20019 で待ち受けし、`probe_single.py` のフル
セッションでも ASan エラーは 0 件だった。通常ビルドでも
`-Wrestrict` 警告が消え、下記プローブで差は出ていない。
なお ASan の調査時に `ASAN_OPTIONS=replace_intrin=0` を
付けると、この重なりは検出されなくなる（第5段階の記録の
「ASan での調査には replace_intrin=0 が要る」は、異常終了
を回避して先の調査を進めるための指定）。

### 見つけた癖（すべて存置）

1. 未使用変数：`load_sav` の `name[32]` / `j` / `k`、
   `load_authority` の `j`、`remap_lock_and_board` の `i`。
   `efclose` の `i` は代入のみ（set-but-unused）。
2. `decimal_load_p` の条件式内の代入（parentheses 警告、
   約 8792 行目）。真理値として使う意図の既存コード。
3. 符号混じり比較 6 件、字下げが誤解を招く箇所 6 件、
   format-truncation 9 件、write-strings 199 件は、いずれも
   書き換え前と同数・同種（詳細は警告内訳の照合どおり）。
4. `load_fd` の `buf2` は `AREA_SUB` の有無でサイズが変わる
   （書き換え内容の項を参照）。プリプロセッサ構造は不変更。
5. このモジュールでは、上記の警告類のほか新規の潜在バグは
   見つからなかった。`load_dd` の重なりは本段階で判断・
   対処済み（前節）。

### 検証

- 文字列・文字リテラルの多重集合が前後で完全一致
  （1534 件）。
- プリプロセッサ行とコメントを除いたトークン列に、意図した
  変換の逆変換（`std::array` 宣言 → C 宣言、`.data()` の
  除去、`.size()` → sizeof、`std::` 限定の除去、nullptr →
  NULL、`kFnc` → `fnc`、memmove の 1 か所を意図した変更
  として扱う）を適用すると前後完全一致（70,245 トークン）。
  git の差分も目視で確認し、意図した変更区分以外は無い。
- バイナリの `strings` 比較：差分は `fnc` の無名名前空間化
  に伴うシンボル名（`_ZL3fnc` → `_ZN12_GLOBAL__N_1L4kFncE`）、
  `std::isspace` 限定に伴う参照シンボルの変化
  （`__ctype_b_loc` → `isspace`）、ジャンプテーブル番号と
  コード生成に伴うバイト列のノイズのみで、メッセージ
  文字列は不変。
- 回帰プローブ（テストワールド、ポート 20019）：
  - `probe_m57.py` 敵なし変種 n：地図バイト・テキスト
    115 行とも完全一致。
  - `probe_single.py`：差分は Cat の徘徊による `#m57 O` 行
    と空白行の位置のみ（書き換え前同士でも同量・同種）。
    保存ファイル `phi.cd` は 1 行のみ差（サイコロ依存の
    ステータス値 1 つと保存時刻の epoch 秒）で、書き換え前
    バイナリ同士の 2 回実行でも同じ 1 行差が出ることを
    確認した（保存形式・内容の非回帰）。
  - `probe_party.py`：サマリ JSON は一致、トランスクリプト
    A は完全一致、B は空白行 1 行のみ。
  - `probe_calc.py`：差分行数は前後 500 行 / 書き換え前
    同士 502 行で同等。差分は NPC 同士の戦闘・詠唱の
    サイコロ結果のみ。
  - `probe_skills.py` base / s1：トランスクリプト・保存
    `:S` 行・phi.log すべて完全一致。s2 は生成アイテム名
    1 件（Spirit claw / Erase claw）のサイコロ差とそれに
    伴う保存差、phi.log の終了時競合 1 行のみ（第8・
    12段階と同じクラス）。
  - `probe_use.py` a / b / c1〜c5 / d / e / disc の 10 変種：
    全変種でトランスクリプト全文一致＋断定 OK。
  - `probe_attack.py`（before×2 / after×2）：スクリプト
    共通部分は同じ分岐点まで一致し、パターン件数の
    ばらつきは書き換え前同士の対照と同等。
  - ASan ビルドでの `load_dd` 検証は前節のとおり。
- 動的に踏めていない経路：世界移動の着側での保存、メール
  系の一部などプローブの脚本に無い経路は、トークン照合と
  書式・引数の照合で確認した。代替ビルド（`AREA_SUB` 等）
  のプリプロセッサ構造はそのまま保ってある。

### C++ 化の完了と申し送り

- 本段階でサーバ本体の全モジュール（`tcp` / `phi_str` /
  `ext_party` / `ext_data` / `new_command` / `new_proto` /
  `phi_m57` / `expression_op` / `phi_calc` / `phi_use` /
  `ext_skills` / `phi_attack` / `phi_enemy` / `phi_magic` /
  `phi_dm` / `phi_data`）の C++ 化が完了した。変換ツール
  `cdcnv.cpp` と BSD 由来の `strlcpy` / `strlcat` は対象外
  （第0段階で C++ としてビルドできる状態にしてある）。
- 修正判断がユーザー待ちの潜在バグは次の 5 件（群）で、
  いずれも振る舞い維持のため存置してある：`OP_REVERSE`
  のクラッシュ（第5段階）、`skill_crt_check` の範囲外
  読み（第8段階）、`speed_degrade` の IT_FOOD →
  IT_JEWEL フォールスルー（第9段階）、`phi_magic` の
  FN_PRISM フォールスルーと `charge_spell` の `md[n]`
  添字ミス（第11段階）、`phi_dm` の `ST_BAN_` の
  sizeof ミス・`check_write` の `buf[-1]` 読み・
  `twobyte_check` の未初期化戻り値（第12段階）。
  `load_dd` の memcpy 重なりは本段階で対処済み。
- GitHub リポジトリ `PHIDM_NEO` への反映は第7段階分まで
  で、第8〜13段階はローカルコミットのみ（スナップショット
  push は承認待ち）。

## 潜在バグ修正（2026-11-16、C++ 化完了後の追補）

ユーザーの判断により、各段階で「振る舞い維持のため存置」
とした潜在バグ 9 件をすべて修正した。`load_dd` の memcpy
重なりは第13段階で対処済みのため含まない。各段階の
「見つけた癖（すべて存置）」の該当記述と、ソース内の
存置コメントは本節が正（コメントは修正済みに更新した）。
検証材料は `~/workspace/phi-cpp-verify/scratch/bugfix/`
に置いた（リポジトリ外）。

### 1. OP_REVERSE のクラッシュ（expression_op.cpp、c8bf10a）

- 修正：`OP_REVERSE` で符号反転した ACUM を push する際、
  `a2.kind = EXP_CONST` を設定するようにした。
- 根拠：第5段階の記録にあった修正案どおり。VAL と ADDRESS
  は共用体を共有するため、変数 (EXP_VARIABLE) のまま
  push すると次回 `get_value` が壊れた ADDRESS を読む。
- 検証：式評価ハーネスの variant C（`x1=1000,
  @CHR.gp=-(x1)` というスイッチ）で、修正前はログイン
  直後に SIGSEGV（phi.log に記録、セッション断）に
  なることを再現し、修正後は落ちずに所持金が
  -1000 gp と正しく評価されることを確認した。
  variant A / B1 / B2 は修正前後で一致。

### 2. skill_crt_check の範囲外読み（ext_skills.cpp、c1677bc）

- 修正：添字を `(id->type & WT_MASK)/WT_PLACE`、
  `(id->type & MT_MASK)/MT_PLACE` に変え、読んだ値を
  割る形をやめた。`skillup_create` と同形。
- 検証：スキル行（16 int）を PROT_NONE のガードページ
  直前に置く単体テストで、type 0x1234 のアイテムに対し
  修正前は行外読みで SEGV（ext_skills.cpp:737）、修正後
  は期待値 227 を返すことを確認した。実サーバ経路
  （probe_skills s2）では PER_DICE の短絡評価で呼び出し
  自体が確率的になるため、ASan ビルドの s2 実行では
  検出されなかった（単体テストが決定的な証拠）。
  なお s2 の生成アイテム名（Shell/Air/Spirit claw）が
  実行ごとに変わるのは `create_special_item` の乱数で、
  同一バイナリの再実行でも変動することを確認済み
  （本修正起因ではない）。

### 3. speed_degrade の IT_FOOD フォールスルー（phi_attack.cpp、e027570）

- 修正：`IT_FOOD` ケースの末尾に `break;` を追加した。
- 根拠：`degrade()` 側は 20010220 の修正で同型の break
  が入っており、そちらに合わせた。食べ物が IT_JEWEL の
  処理（耐久-1、ATP 15/16 倍、VALUE 半減）を受ける
  のは誤り。
- 検証：probe_attack のパターン比較で非回帰（差は戦闘
  ダイス由来のみ）。

### 4. mg_attack_to の FN_PRISM フォールスルー（phi_magic.cpp、06f0a04）

- 修正：`FN_PRISM` ケースに `break;` を追加し、計算した
  tfn（被攻撃側 dfn+1、FN_NORMAL 超は FN_EARTH）が
  default の `tfn = m->fn` で捨てられないようにした。
- 検証：全スイート非回帰。PRISM 属性の魔法攻撃を決定的
  に踏むプローブは無いため、効果はコード上の確認。

### 5. charge_spell の md[n] 添字ミス（phi_magic.cpp、06f0a04）

- 修正：extension 連結時の `cd[n].md[n].extension` を
  `cd[n].md[m].extension`（呪文番号 m）に直した。
- 検証：全スイート非回帰。extension 持ちの武器と呪文
  での charge が必要で、プローブ範囲外のため効果は
  コード上の確認。

### 6. ST_BAN_ の sizeof ミス（phi_dm.cpp、3f2899e）

- 修正：`dmprintf(n, 0, rbuf, sizeof(rbuf), ...)` の長さを
  1024 に直した（ポインタの sizeof = 8 で返信が先頭
  8 バイトに切り詰められていた）。
- 根拠：rbuf は `input_analyze` の `char *` 引数で、実
  呼び出しはすべて main() の `char buf[1024]`。同関数内
  の他の rbuf 書き込みも 1023 を直書きし「main() の
  buf サイズに依存」とコメント済み。dmprintf の len は
  バッファ容量（len-1 まで整形）で、他の呼び出しも
  バッファ全体の大きさを渡している。
- 検証：静的確認 + 全スイート非回帰（BAN 解除コマンド
  を踏むプローブは無い）。

### 7. check_write の buf[-1] 読み（phi_dm.cpp、3f2899e）

- 修正：末尾改行チェックを `buf[0] != '\0' && ...` と
  ガードした。空の mes はそのまま渡り、`write_line`
  が 0 バイト送信する（従来は buf[-1] を読んでいた）。
- 検証：静的確認 + 全スイート非回帰（空文字列を渡す
  呼び出しは現行コードに無い）。

### 8. twobyte_check の未初期化戻り値（phi_dm.cpp、3f2899e）

- 修正：戻り値 `m` を 0 で初期化した。
- 根拠：呼び出し側は戻り値 0 を「有効な文字が無い＝
  不合格」として名前を拒否しており、通常経路（空で
  ない文字列）の戻り値とも矛盾しない。
- 検証：静的確認 + 全スイート非回帰。

### 9. fn() の境界外読み（phi_calc.cpp、a597e85）

- 修正：表引きの前に `i %= 6` を入れ、負側の `i += 6`
  と合わせて完全な剰余正規化にした。
- 根拠：属性の相性は 6 つで循環し、元の `i += 6` は
  その 1 回分の調整だった。範囲外を既定値に落とす
  より、剰余に丸める方が元の意図に近い。範囲内
  （afn/dfn が 0..9）の入力では結果は従来と同一。
- 検証：単体テストで afn/dfn 0..9 の全 100 通りが
  修正前と完全一致すること、範囲外では修正前が境界外
  読みのゴミ値（例：fn(100,1,1000) = -137790）を
  返すのに対し、修正後は正規化値（1000）を返す
  ことを確認した。

### 回帰検証（全体）

修正前バイナリ（eba4842）と修正後で全プローブを同一
条件で実行し比較した。残った差はすべて従来から知られた
非回帰の型（敵の徘徊・戦闘ダイス・接続スロット番号・
HP regen・phi.log のタイミング差）のみ：

- probe_single：敵（Cat）の徘徊行のみ差。
- probe_m57（敵なし）：地図バイト完全一致。テキスト差
  は DM 負荷の数値のみ。
- probe_party：サマリ・両記録とも完全一致。
- probe_calc：スクリプト応答は一致（差は敵に進路を
  塞がれた「進めません」2 行）。
- probe_skills base / s1：記録・保存 :S 行・phi.log
  すべて完全一致。s2：接続スロット番号・HP regen・
  生成アイテム名（乱数）のみ差。
- probe_use：全バリアント OK（a の phi.log 空行位置と
  d のゲート閉鎖通知はタイミング差。再実行ペアでは
  d も OK）。
- probe_attack：パターン比較で戦闘ダイス由来の差のみ。
- 式評価 A / B1 / B2：一致。C：上記 1. のとおり。

## 9x9 マップ対応（2026-11-16、PHIDM_NEO v1.1）

C++ 化完了後の新機能として、9x9 サイズの地図表示を追加した。
これはプロトコルの拡張であり、従来の 5x5 / 7x7 の振る舞いは
変えていない。

### プロトコル

- クライアントが `#ex-map size=81` を送ると、サーバはモード
  フラグ MD_M81 (0x40) を立て、`#ex-map size=81` と返して以後
  `#m81` 形式で地図を送る。`size=57` / `size=40` を送れば従来
  形式に戻る。`mapsize_only55` の世界では 57 と同様に断る
  (`#ex-map size=no`)。
- `#m81` の行書式は `#m57` と同一の約束 (M 行は行頭 18 バイト
  + セル 2 バイト × 81 で 181 バイト、O 行の欄配置も同じ、
  終端は `#m81 .`)。
- Eagle eye は、接続中クライアントが M81 のとき 20x20
  (size=19、範囲 -10..+9) で送る。それ以外は従来通り
  (FF_M57 の土地では 11x11、それ以外は 15x15)。
  ※ 当初 M81 では 11x11 にしていたが、2026-11-17 に仕様変更:
  9x9 の方が 7x7 (15x15) より広く見えるべき、とされて 20x20
  になった。20 は偶数なので中心は1マス北西寄りになる。

### 実装

- 新規ファイル phi_m81.cpp / phi_m81.h に、phi_m57.cpp の
  9x9 版 (set_view9 / print_m81) を置いた。セルの 2 バイト変換
  (dd_m57) と床アイテム種別 (flooritemtype) は m57 側を共用。
- 視界の形: 北固定 (solid) では自キャラが中心 (4,4)。回転
  (turn) では前方 5・後方 3 で、自キャラは (4,5) に来る
  (m57 の turn が前方 4・後方 2 なのと同じ考え方)。
- 可視判定 (is_visible9) は m57 と同じ規則。走査順序は中心
  からのマンハッタン距離の昇順に生成し、m57 の表と同じ前提
  (対象より中心に近いセルが先に決まる) を満たす。
- print_map の振り分け 2 か所に M81 を追加。#name 再送の抑止
  (phi_data.cpp のスイッチ 2 件、phi_dm.cpp の charagra_set)
  と名前/グラ名の選択 (phi_dm.cpp) も M57 と同条件に M81 を
  加えた。クライアント乗り換え時 (phi_dm.cpp) は MD_M57 と
  一緒に MD_M81 も落とす。

### 互換性と制限

- 旧クライアントは size=81 を送らないので影響を受けない。
  新クライアント (NeoPHI) は、81 を要求しても 9x9 でない地図
  が届いた場合に 7x7 へ自動で落とす実装になっている。
- FF_M57 の土地向けの「向きに応じた端の視界制限」は、9x9
  では適用していない (9x9 は新しい土地向けのモードという
  位置付け)。

### 検証

- ローカルのテスト世界 (20017) へ生 TCP で接続し、size=81
  受付、`#m81 M` 行が 181 バイトで届くこと、移動で再送される
  こと、solid / turn 両様式で自キャラ位置が (4,4) / (4,5) に
  なること、size=57 へ戻すと従来の `#m57` (117 バイト) に
  戻ることを確認した。
- NeoPHI クライアント実機でも 81 セルの描画を確認した。

## 2026-11-17: 書き込みバッファ溢れ切断の緩和 (tcp.cpp)

掲示板の検索結果のような大量行を送る途中、クライアント側の
受信が一瞬滞ると、サーバの書き込みバッファ (16KB) が溢れた
瞬間に待ち時間 0 で一度だけ送出を試して失敗し、そのまま
切断していた (CONNECT_BUFFULL)。WPHI など他のクライアント
でも同じ切断が起きる、サーバ側の挙動である。

- 書き込みバッファを 16KB から 64KB へ拡大した。
- write_line でバッファが溢れた時は、指定待ち時間が 0 でも
  200ms まで待って吐き出しを試みるようにした。本当に死んだ
  相手は、その待ちの後に従来通り切断される。

検証: ローカルのテスト世界 (20017) で新バイナリを起動し、
通常のログインと #m81 地図の受信が従来通りなことを生 TCP
で確認した。大量行そのものの再現はテスト世界の掲示板に
記事が無いため未実施 (緩和の理屈はコード上で確認)。

## 2026-11-17: 2x2 の大きな敵 (phi.ed :O B)

Gigant 判定 (mhp 5000 以上など) とは別に、敵マスタデータ
phi.ed の :O オプションに B を書いた敵だけが 2x2 の体を
持つようにした。基準マス (出現位置) から東・南・南東の
4 マスを体とする。ブロックは向きに合わせて回転しない
(基準マス固定)。

- データ: CHROData / CHRData に big 旗を追加。load_ed_o が
  :O の B で立て、load_ed が出現時に引き継ぐ。判定は
  chr_big(n) (phi_calc.cpp)。
- 登録: set_chr / clear_chr が 4 マス分の chr リストを
  登録・解除する (embed_chr)。これにより「見る・狙う・
  占有」の全経路が 4 マス前提に揃う。
- 移動: move_chr は大きな敵の体を誰にとっても進入不可に
  し、大きな敵自身の移動では行き先の 4 マス全ての地形と
  占有を検証する (1 マスでも塞がれば動けない)。
- 攻撃: attack_other は大きな敵の正面攻撃を前縁 2 マスに
  広げた (attack_line)。通常キャラから大きな敵への攻撃は、
  体のどのマスを狙っても従来の経路で当たる。敵 AI の
  attack_check も前縁 2 マスを攻撃可能と判定する
  (big_target_in_dir)。大きな敵同士が向き合うと前縁 2 マス
  の両方から当たるため 2 回攻撃になる (既知の制限)。
- 出現: load_ed の出現位置は 4 マス全てが地形的に有効で
  完全に空いている場合のみ許可する。通常の敵も、大きな
  敵の体のマスには出現しない。
- 地図送信: set_view / set_view7 / set_view9 で、大きな敵は
  基準マスのセルだけ視界に残す (他の体のマスは消す)。同じ
  敵が 4 回送られることはない。大きさ印は chr_marker() で
  'B' を出す ('*' は gigant 専用のまま)。WPHI はこの印を
  読むだけで使わないため互換性に影響しない。NeoPHI は
  'B' を見て 2x2 に描く。
- 魔法: 範囲・直線魔法 (mg_heal / mg_sphere_attack /
  mg_explode / mg_line_attack) は、大きな敵の体の複数マス
  が範囲に入っても 1 回だけ適用する (big_already)。
  視界 (view) 経由の魔法は視界側の重複除去で 1 回になる。

検証: テスト世界 (20031、phi.ed に :O B のテストモンスター
を追加) で Python クライアントから実地確認した。出現
(4 マス空きチェック、半径内に有効な場所が無いと出ない)、
4 マス表示、体への侵入不可、基準マス・非基準マスどちらの
前マスからでも殴れること、横側の前マスに立つキャラへの
反撃 (従来判定では届かない位置)、移動、死亡時の登録解除
(跡地に他キャラが立てる)、通常の敵への影響なし、地図で
1 回だけ 'B' 印で届くこと。範囲魔法の重複防止はコード
確認のみ (対応する魔法を持つキャラでの実地は未実施)。
