/* for Phi additional commands */
/* written by Qajin */

/* 追加コマンドの振り分けと、掲示ファイルの表示。
 *
 * command_analyze_ex() は command_analyze() から呼ばれ、ここで
 * 扱うコマンドなら処理して 0 を、扱わないなら 1 を返す。
 * 1 が返ると、呼び出し側で通常のコマンド・発言として処理される。
 * なお、ここで処理されたコマンドは command_analyze() の
 * gasify 解除 (CN_GASIFY(n) = 0) を通らない。join/bye/guard/
 * show/skill を打っても無操作タイマは解除されない。
 */

#include <array>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

#include "new_command.h"
#include "phi_dm.h"
#include "phi_calc.h"
#include "phi_data.h"
#include "phi_str.h"
#include "phi_attack.h"
#include "ext_skills.h"
#include "ext_party.h"

int command_analyze_ex(int n, char *p)
{
    /* add 20010703 */
    if (std::strcmp(p, "guard") == 0) {
        if (TCN_PALSY(n)) {
            dmcheck_write(n, DMM_MOVE_NO);
            return 0;
        }
        *(cd[n].com) = 'G';
        guard_self(n);
        return 0;
    } else
    /* add for skills 20020502 */
    /* status_skills() が 0 以外を返した場合は次の分岐へ落ちる。
     * skill で始まる入力は show/join/bye のどれでもないので、
     * 最終的に 1 を返し、発言などとして扱われる。 */
    if (std::strncmp(p, "skill", 5) == 0 && !status_skills(n, p + 5)) {
        return 0;
    } else if (std::strncmp(p, "show ", 5) == 0) {
        SKIP_TO_SPACE(p);
        SKIP_SPACE(p);
        show_file(n, p);
        return 0;
    } else if (std::strcmp(p, "join") == 0) {
        ask_join(n);
        return 0;
    } else if (std::strcmp(p, "bye") == 0) {
        bye_party(n);
        return 0;
    }
    return 1;
}

/* phi_shows/ 以下の掲示ファイルを表示する。
 * ファイル名に使えるのは英小文字・数字・_ だけで、それ以外の
 * 文字は読み飛ばすため、ディレクトリを抜け出すことはできない。
 * 名前が空なら何もしない。ファイルが無くても 1 を返す
 * (bulletin_board_dm() が空行だけ送って戻る)。
 */
int show_file(int n, char *p)
{
    if (p == nullptr) {
        return 0;
    }

    /* 従来は 256 バイトの固定バッファに詰めていた。名前が長いと
     * 末尾の '\0' がバッファを1バイトはみ出す作りだったため、
     * std::string にして同じ 256 文字での打ち切りを再現する。 */
    std::string path = "phi_shows/";
    for (; *p != '\0' && path.size() < 256; ++p) {
        if ((*p >= 'a' && *p <= 'z') ||
            (*p >= '0' && *p <= '9') ||
            *p == '_') {
            path.push_back(*p);
        }
    }

    if (path.size() == 10) {
        return 0;
    }

    bulletin_board_dm(n, path.data());
    return 1;
}


/* add for dm sequence in SHOW 20041208 */
/* 掲示ファイルを1行ずつ送る。前後に空行を入れる。
 * ファイルが開けなければ、最初の空行だけ送って戻る。 */
void bulletin_board_dm(int n, char *fnm)
{
    std::array<char, 256> buf{};

    check_write(n, "\n");
    /* efopen/efclose は fopen/fclose の薄いラッパー
     * (行数カウンタ lcnt の管理つき)。unique_ptr で閉じ忘れを防ぐ。 */
    std::unique_ptr<FILE, void (*)(FILE *)> fp(efopen(fnm, "rb"), &efclose);
    if (fp == nullptr) {
        return;
    }
    while (efgets(buf.data(), 255, fp.get()) != nullptr) {
        dmcheck_write(n, buf.data());
    }
    /* 従来と同じ順序で閉じてから、末尾の空行を送る。 */
    fp.reset();
    dmcheck_write(n, "\n");
}
