/* for Phi extend protocols */
/* written by Qajin */

/* 拡張プロトコル (#ex-*) の処理。
 *
 * func_analyze_ex() は接続中のクライアントからの #ex-map /
 * #ex-switch を処理し、地図の形式 (57/40・solid/turn) と
 * 鷹の目表示の切り替えを行う。DM 演出 (FX) の能力申告
 * (#ex-switch fx=1) と画像要求 (#ex-fx get) もここで受ける。ex_notice() は土地・エリア名の
 * 通知、ex_version_dm() はサーババージョンの通知。
 * ex_register() / ex_adduser_v52() / ex_adduser_v52_finish()
 * は #ex-register による新規登録の手順 (start → プロパティ行
 * → end)、ex_get() は #ex-get による登録情報の問い合わせを
 * 処理する。
 *
 * 呼び出しは phi_dm.cpp の振り分けから。#ex-register の end で
 * 登録が完了すると ex_register() は 1 を返し、呼び出し側が
 * ようこそ手順 (土地通知・掲示・初回地図など) を送る。
 */

#include <array>
#include <cstdio>
#include <cstring>

#include "new_proto.h"
#include "tcp.h"
#include "phi_dm.h"
#include "phi_data.h"
#include "phi_str.h"
#include "phi_m57.h"
#include "phi_class.h" /* pet system: #ex-pet senders (PHIDM NEO extension) */
#include "ext_data.h"

extern int reflesh_last_map[MAX_NTTYS];

namespace {

/* グラフィックが変わっている場合だけ #name で表示名を送り直す。
 * 従来の3か所は送る名前が違う (size=57 を受け入れたときだけ
 * キャラ名、それ以外はグラ名) ので、名前は呼び出し側が渡す。 */
void send_name_if_cg_changed(int n, const char *name)
{
    if (IS_CGCHGED(n)) {
        std::array<char, 256> buf{};
        snprintf(buf.data(), buf.size(), "#name %s\n", name);
        check_write(n, buf.data());
    }
}

} // namespace

/* #ex-map / #ex-switch を処理する。戻り値は常に 0。
 * プロパティは get_property() で順に取り出し、未知のものは
 * 読み飛ばす。 */
int func_analyze_ex(int n, char *p)
{
    std::array<char, 32> protocol{};
    std::array<char, 32> property{};
    std::array<char, 32> value{};

    loadstr(protocol.data(), &p, ' ', static_cast<int>(protocol.size()));
    SKIP_SPACE(p);

    if (std::strcmp(protocol.data(), "#ex-map") == 0) {
        while (get_property(&p, property.data(),
                            static_cast<int>(property.size()), value.data(),
                            static_cast<int>(value.size()))) {
            if (std::strcmp(property.data(), "size") == 0) {
                if (std::strcmp(value.data(), "57") == 0) {
                    if (mapsize_only55) {
                        /* 57 桁地図を許可しない世界では断る。 */
                        cd[n].modef &= ~MD_M57;
                        cd[n].modef &= ~MD_M121;
                        check_write(n, "#ex-map size=no\n");
                        send_name_if_cg_changed(n, CHRGRA(n));
                    } else {
                        cd[n].modef |= MD_M57;
                        cd[n].modef &= ~MD_M121;
                        check_write(n, "#ex-map size=57\n");
                        send_name_if_cg_changed(n, cd[n].name);
                    }
                } else if (std::strcmp(value.data(), "81") == 0) {
                    /* 9x9 (旧 M81) は仕様削除 (2026-11-20、11x11 に
                     * 統一)。81 の要求は断って従来地図にする。 */
                    cd[n].modef &= ~MD_M57;
                    cd[n].modef &= ~MD_M121;
                    check_write(n, "#ex-map size=no\n");
                    send_name_if_cg_changed(n, CHRGRA(n));
                } else if (std::strcmp(value.data(), "121") == 0) {
                    if (mapsize_only55) {
                        /* 121 桁地図を許可しない世界では断る。 */
                        cd[n].modef &= ~MD_M57;
                        cd[n].modef &= ~MD_M121;
                        check_write(n, "#ex-map size=no\n");
                        send_name_if_cg_changed(n, CHRGRA(n));
                    } else {
                        cd[n].modef |= MD_M121;
                        cd[n].modef &= ~MD_M57;
                        check_write(n, "#ex-map size=121\n");
                        send_name_if_cg_changed(n, cd[n].name);
                    }
                } else if (std::strcmp(value.data(), "40") == 0) {
                    cd[n].modef &= ~MD_M57;
                    cd[n].modef &= ~MD_M121;
                    check_write(n, "#ex-map size=40\n");
                    send_name_if_cg_changed(n, CHRGRA(n));
                }
                /* 57 でも 40 でもない値でも、返事はせずに地図
                 * だけ描き直す (従来通り)。 */
                reflesh_last_map[n] = 1;
                print_map(n, 0);
            } else if (std::strcmp(property.data(), "style") == 0) {
                if (std::strcmp(value.data(), "solid") == 0 &&
                    turn_permission <= 0) {
                    cd[n].modef |= MD_M57SOLID;
                    check_write(n, "#ex-map style=solid\n");
                } else if (std::strcmp(value.data(), "turn") == 0 &&
                           turn_permission >= 0) {
                    cd[n].modef &= ~MD_M57SOLID;
                    check_write(n, "#ex-map style=turn\n");
                } else {
                    /* 受け付けられない値や世界の許可に合わない
                     * 要求は断り、残りのプロパティは処理せず戻る。 */
                    check_write(n, "#ex-map style=no\n");
                    return 0;
                }
                reflesh_last_map[n] = 1;
                print_map(n, 0);
            }
        }
    } else if (std::strcmp(protocol.data(), "#ex-switch") == 0) {
        while (get_property(&p, property.data(),
                            static_cast<int>(property.size()), value.data(),
                            static_cast<int>(value.size()))) {
            if (std::strcmp(property.data(), "eagleeye") == 0) {
                if (std::strcmp(value.data(), "form") == 0) {
                    cd[n].modef |= MD_FEAGLE;
                    check_write(n, "#ex-switch eagleeye=form\n");
                } else if (std::strcmp(value.data(), "text") == 0) {
                    cd[n].modef &= ~MD_FEAGLE;
                    check_write(n, "#ex-switch eagleeye=text\n");
                }
                /* form でも text でもなければ何もしない。 */
            } else if (std::strcmp(property.data(), "fx") == 0) {
                if (std::strcmp(value.data(), "1") == 0) {
                    cd[n].modef |= MD_FX;
                    check_write(n, "#ex-switch fx=1\n");
                }
                /* 1 でなければ何もしない。 */
            } else if (std::strcmp(property.data(), "height") == 0) {
                if (std::strcmp(value.data(), "1") == 0) {
                    cd[n].modef |= MD_HEIGHT;
                    check_write(n, "#ex-switch height=1\n");
                }
                /* 1 でなければ何もしない。 */
            } else if (std::strcmp(property.data(), "fastmove") == 0) {
                if (std::strcmp(value.data(), "1") == 0) {
                    cd[n].modef |= MD_FASTMOVE;
                    set_tcp_nodelay(n);
                    check_write(n, "#ex-switch fastmove=1\n");
                }
                /* 1 でなければ何もしない。 */
            } else if (std::strcmp(property.data(), "pet") == 0) {
                if (std::strcmp(value.data(), "1") == 0) {
                    cd[n].modef |= MD_PET;
                    check_write(n, "#ex-switch pet=1\n");
                    /* 申告したクライアントへ現在のペット状態を
                     * まとめて送る (PHIDM NEO extension) */
                    pet_send_self(n);
                    pet_send_chr(n);
                    pet_send_chr_all(n);
                }
                /* 1 でなければ何もしない。 */
            } else if (std::strcmp(property.data(), "job") == 0) {
                if (std::strcmp(value.data(), "1") == 0) {
                    cd[n].modef |= MD_JOB;
                    check_write(n, "#ex-switch job=1\n");
                    /* 申告したクライアントへ現在の職業・性別を
                     * まとめて送る (PHIDM NEO extension) */
                    jb_send_self(n);
                    jb_send_chr(n);
                    jb_send_chr_all(n);
                }
                /* 1 でなければ何もしない。 */
            }
        }
    } else if (std::strcmp(protocol.data(), "#ex-fx") == 0) {
        /* #ex-fx get <name>: FX 画像データの要求への応答
         * (PHIDM NEO extension)。 */
        std::array<char, 32> sub{};
        std::array<char, 32> name{};
        loadstr(sub.data(), &p, ' ', static_cast<int>(sub.size()));
        SKIP_SPACE(p);
        if (std::strcmp(sub.data(), "get") == 0) {
            loadstr(name.data(), &p, ' ', static_cast<int>(name.size()));
            fx_send_image_data(n, name.data());
        }
    }
    return 0;
}

/* 拡張通知を送る。f は 1=土地名、2=土地キー、3=エリア名。
 * 未知の f では -1 を返す。 */
int ex_notice(int n, int f)
{
    switch (f) {
        case 1: /* land */
            dmcheck_write(n, "#ex-notice land=%land\n");
            break;
        case 2: /* landkey */
            /* 未実装(現時点で必要性0と判断) */
            break;
        case 3: /* area */
            dmcheck_write(n, "#ex-notice area=%area\n");
            break;
        default:
            return -1;
    }
    return 0;
}

/* クライアントのバージョンが 5100000 以上のときに、サーバの
 * バージョンを知らせる。 */
void ex_version_dm(int n)
{
    std::array<char, 256> buf{};

    if (cd[n].cv >= 5100000) { /* for old clients ... */
        snprintf(buf.data(), buf.size(), "#version-dm %s\n", PHI_DM_VERS);
        check_write(n, buf.data());
    }
}

/* add for new register user protocol 20050414 */
/* #ex-register プロトコルを処理 */
/* 登録完了時に 1 が返る */
int ex_register(int n, char *p)
{
    SKIP_SPACE(p);

    if (std::strcmp(p, "start") == 0) {
        /* 登録の開始。接続情報を初期化して登録待ち状態へ。 */
        init_connected_cd(n);
        cd[n].str[0] = '\0'; /* name init */
        cd[n].st = ST_USERREGISTER;
        cd[n].wt = 600; /* 時間延長 */
    } else if (std::strcmp(p, "cancel") == 0) {
        /* 登録の中止。初期化して通常のコマンド待ちへ戻る。 */
        init_connected_cd(n);
        cd[n].st = ST_COMMAND;
    } else if (std::strcmp(p, "end") == 0) {
        if (cd[n].st == ST_USERREGISTER) {
            if (ex_adduser_v52_finish(n) == 0) {
                /* 登録完了。呼び出し側がようこそ手順を送る。 */
                return 1;
            }
            /* 失敗 (拒否文を送り済み) は下で ST_COMMAND に戻る。 */
        }
        cd[n].st = ST_COMMAND;
    }
    return 0;
}

/* #ex-register 中の処理。登録待ちの間、クライアントから届く
 * プロパティ行 (name=/mail=/pass=/image=) を取り込む。
 * 値のエラー通知は end 時に行う。 */
int ex_adduser_v52(int n, char *p)
{
    std::array<char, 32> property{};
    std::array<char, 64> value{};
    char *p2;

    while (get_property(&p, property.data(),
                        static_cast<int>(property.size()), value.data(),
                        static_cast<int>(value.size()))) {
        if (std::strcmp(property.data(), "name") == 0) {
            /* 名前は二重空白までを 31 文字まで取り込む
             * (cd[n].str は 256 バイトあるが、ここでの上限は 32)。
             * 先頭の小文字は大文字にする。空なら '\0' のまま。 */
            p2 = value.data();
            loadstr(cd[n].str, &p2, DSPACE, 32);
            cd[n].str[0] = ftoupper(cd[n].str[0]);
        } else if (std::strcmp(property.data(), "mail") == 0) {
            CPYSTR(cd[n].adr, value.data());
        } else if (std::strcmp(property.data(), "pass") == 0) {
            /* ID は 世界識別子 + シリアル5桁 + パスワード6文字。
             * %-6.6s で6文字に詰められ、短い分は空白で埋まる
             * (finish 側で空白までの長さを見て弾く)。 */
            snprintf(cd[n].uid, sizeof(cd[n].uid), "%s%05ld%-6.6s",
                          addw, s_id, value.data());
        } else if (std::strcmp(property.data(), "image") == 0) {
            /* %li なので 10進・8進・16進のどれでも読める。
             * 読めなければ vf は変わらない (戻り値は見ない)。 */
            std::sscanf(value.data(), "%li", &cd[n].vf);
        }
    }

    return 0;
}

/* #ex-register の end で、取り込んだ登録内容を検証する。
 * 問題があれば拒否文を送って -1 を、通れば new_user() で
 * キャラを作って 0 を返す。 */
int ex_adduser_v52_finish(int n)
{
    char *p;
    int f_name;
    int f_pass;
    int f_image;
    int f_mail;
    std::array<char, 256> buf{};
    int i;

    f_name = f_pass = f_image = f_mail = 0;

    /* name: 2文字以上なければ拒否。 */
    if (cd[n].str[0] == '\0' || cd[n].str[1] == '\0') {
        f_name = 1;
    }

    /* pass: uid の9バイト目 (addw 4文字 + シリアル5桁の後ろ)
     * から空白までを切り詰め、6文字ちょうどでなければ拒否。
     * %-6.6s の空白埋めと合わせ、6文字ちょうど・空白なしの
     * パスワードだけが通る。fisspace() は '\0' を空白と
     * みなさないので、末尾の番兵チェックが要る。 */
    p = &cd[n].uid[9]; /* pass出だし */
    while (!fisspace(*p)) {
        if (*p == '\0') {
            break;
        }
        ++p;
    }
    *p = '\0';
    if (std::strlen(cd[n].uid + 9) != 6) {
        f_pass = 1;
    }

    /* image: 初期グラ一覧がある世界では、番号が範囲内か見る。
     * 範囲内なら対応するグラ名を ocg にする。一覧が無い世界
     * では何もしない。 */
    if (init_graph_n) {
        if (cd[n].vf < 0 || cd[n].vf >= init_graph_n) {
            f_image = 1;
        } else {
            cd[n].ocg = findstr(init_graphic[cd[n].vf]);
        }
    }

    /* mail は 空でもよし */
    /* 登録受付はするが転送はしない */
    /* '@' が無ければ末尾に '@' を付ける (formatA保護)。
     * 末尾2バイト分の余裕を残して探すので、'@' と '\0' が
     * 必ず収まる。 */
    for (i = 0, p = &cd[n].adr[0];
         i < sizeof(cd[n].adr) - 2 && *p != '\0' && *p != '@';
         ++i, ++p) {
        /* '@' か末尾まで空回し */
    }
    if (*p != '@') {
        /* @ がない -> 末尾に@を付ける(formatA保護) */
        *p++ = '@';
        *p = '\0';
    }

    /* f_mail が立つことはない (mail は空でも通す) ので、
     * 拒否文の mail 欄は常に空。形は従来のまま残す。 */
    if (f_pass || f_name || f_image || f_mail) {
        /* 書式自体に末尾の改行は無いが、check_write() が補う
         * ので、実際に送られる行には改行が付く。なお元の
         * ソースでは " name" の直後にタブ文字が見えるが、あれ
         * は三項演算子の : の前の空白であって、文字列の一部
         * ではない (各欄は " name" のような形そのまま)。 */
        snprintf(buf.data(), buf.size(), "#ex-register reject%s%s%s%s",
                      f_name ? " name" : "",
                      f_pass ? " pass" : "",
                      f_image ? " image" : "",
                      f_mail ? " mail" : "");
        check_write(n, buf.data());
        return -1;
    }

    new_user(n);
    return 0;
}

/* #ex-get 処理。REGINFO IMG の問い合わせに、初期グラ一覧を
 * #ex-put 行で返す。各行の末尾の改行は check_write() が補う。 */
void ex_get(int n, char *p)
{
    SKIP_SPACE(p);

    if (std::strncmp(p, "REGINFO", 7) == 0) {
        p += 7;
        SKIP_SPACE(p);
        if (std::strncmp(p, "IMG", 3) == 0) {
            check_write(n, "#ex-put REGINFO IMG");
            for (int i = 0; i < init_graph_n; ++i) {
                check_write(n, init_graphic[i]);
            }
            check_write(n, "#ex-put .");
        }
    }
}
