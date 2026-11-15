/* for Phi join system */
/* written by Qajin */

/* パーティ(仲間)への勧誘と離脱。
 *
 * 勧誘の流れ:
 *   1. 誘う側が join コマンドを打つと ask_join() が目の前のマスに
 *      いるキャラを調べ、応答できる相手に誘いをかける。
 *   2. 誘われた側は ST_JOIN_ANSWER、誘った側は ST_JOIN_INVITE になる。
 *      誘われた側が y と答えればパーティ結成、それ以外なら辞退
 *      (応答の処理は phi_dm.cpp の ST_JOIN_ANSWER の項)。
 *   3. 誘った側が別の入力をすると勧誘は取り消される
 *      (phi_dm.cpp の ST_JOIN_INVITE の項)。
 *
 * パーティの番号は、結成時の代表者(誘った側)のキャラ番号である。
 * renumber_party() は離脱の後に番号を振り直し、1人だけ残った
 * 場合はパーティを解消する。
 */

#include <array>
#include <cstdio>

#include "ext_party.h"
#include "phi_dm.h"
#include "phi_str.h"

extern int px[];
extern int py[];

/* 目の前のマスにいるキャラへ仲間になるよう誘いをかける。
 * 戻り値は常に 0 (呼び出し側は使わない)。
 * 誰もいなければ DMM_JOIN_NOBODY を誘う側へ送る。
 */
int ask_join(int n)
{
    /* 誘う側の向いている先のマス。px/py は向きごとの移動量。 */
    const MapData *front =
        &map[cd[n].y + py[cd[n].d]][cd[n].x + px[cd[n].d]];
    std::array<char, 256> buf{};
    int found = 0;

    /* chr は 1 始まりのキャラ番号で、0 は空きを表す。 */
    for (const auto entry : front->chr) {
        const int j = entry - 1;
        if (j < 0 || j >= MAX_NTTYS) {
            continue;
        }
        ++found;
        if (cd[j].st != ST_COMMAND) {
            /* 戦闘中や入力待ちなど、応答できない状態 */
            dmprintf(n, j, buf.data(), static_cast<int>(buf.size()),
                     DMM_JOIN_BUSY);
            check_write(n, buf.data());
        } else if (CN_GASIFY(j) >= 60) {
            /* 長く無操作で、応答がないものとして扱う */
            dmprintf(n, j, buf.data(), static_cast<int>(buf.size()),
                     DMM_JOIN_NORESPONSE);
            check_write(n, buf.data());
        } else if (cd[j].party != 0) {
            /* すでにどこかのパーティに入っている */
            dmprintf(n, j, buf.data(), static_cast<int>(buf.size()),
                     DMM_JOIN_ALREADY);
            check_write(n, buf.data());
        } else {
            /* 勧誘をかける。相手は応答待ち、自分は勧誘中に移る。
             * gasify に 0x10000 を立てて勧誘中の目印にする
             * (応答・辞退のときに本体側で 0 に戻される)。
             * vf に誘った側の番号を残し、応答側が相手を知る。 */
            cd[j].st = ST_JOIN_ANSWER;
            CN_GASIFY(j) = 0x10000;
            cd[j].vf = n;
            cd[n].st = ST_JOIN_INVITE;
            CN_GASIFY(n) = 0x10000;
            dmprintf(n, j, buf.data(), static_cast<int>(buf.size()),
                     DMM_JOIN_INVITE);
            write_companion(n, j, buf.data(), 0);
            dmcheck_write(j, DMM_JOIN_ASK);
            check_write(j, "#s-edit\n");
        }
    }

    if (found == 0) {
        dmcheck_write(n, DMM_JOIN_NOBODY);
    }

    return 0;
}

/* パーティ番号 n の番号を振り直す。
 * メンバーの party を先頭のメンバーの番号にそろえる。1人だけ
 * 残った場合はパーティとして意味がないので解消する。
 * n がキャラ番号の範囲外なら何もしない。
 */
void renumber_party(int n)
{
    if (n < 1 || n > MAX_NTTYS) {
        return;
    }

    int members = 0;
    int first = 0;
    for (int i = 1; i < MAX_NTTYS; ++i) {
        if (cd[i].party == n) {
            ++members;
            if (first == 0) {
                first = i;
            }
            cd[i].party = first;
        }
    }
    if (members == 1) {
        cd[first].party = 0;
    }
}

/* パーティから抜ける。抜けた後、残りのメンバーの番号を振り直す。
 * パーティに入っていないときはその旨を送るだけ
 * (bug fix for no joined bye 20030519)。
 */
void bye_party(int n)
{
    if (cd[n].party != 0) {
        std::array<char, 256> buf{};
        dmprintf(n, 0, buf.data(), static_cast<int>(buf.size()),
                 DMM_PART_PARTY);
        write_companion(n, 0, buf.data(), 3);

        const int party = cd[n].party;
        cd[n].party = 0;
        renumber_party(party);
    } else {
        dmcheck_write(n, DMM_PART_NO);
    }
}
