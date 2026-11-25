/* for Phi extend data */
/* written by Qajin */

/* 拡張データ(プロパティ文字列)の読み書き。
 *
 * プロパティ文字列は、空白で区切られた key=value の並びである。
 * 値は引用符で囲むことができ、空白を含む値を書ける:
 *   key=value  key="a b"  key='a b'
 * 先頭の空白は読み飛ばす。行末(\0 または \n)で終わりである。
 *
 * 戻り値の約束に注意: 一致・成功は -1、 不一致・失敗は 0 を返す
 * 関数がある (match_property / match_oneproperty / wildcmp)。
 * これは真値を -1 とする古い流儀で、0 は偽である。get_property /
 * set_property / get_propertyvalue は成功を 1、失敗を 0 とする。
 *
 * 使われ方: new_proto.cpp がクライアントからの #ex-... 行を
 * get_property() で分解する。phi_data.cpp がアイテム・魔法の
 * 拡張データを match_property() / set_property() で比べ・作り、
 * phi_use.cpp が get_propertyvalue() で資源指定を読む。
 */

#include <array>
#include <cstdio>
#include <cstring>

#include "phi_dm.h"
#include "phi_data.h"
#include "phi_str.h"
#include "ext_data.h"

/* プロパティ文字列 *p から先頭の1項目を取り出す。
 * property と value に書き込み、*p を次の項目の手前まで進める。
 * 項目が取れれば 1、もう項目がなければ 0 を返す。
 * p が NULL、*p が NULL、残りが空白だけか空行のときも 0 を返す。
 * 値の区切り: " で始まれば次の " まで、' で始まれば次の ' まで、
 * それ以外は空白まで。区切り文字そのものは値に含まれない。
 * loadstr() は区切り文字の次まで *p を進める。ただし取り出した
 * 文字列が長さ制限に収まらない場合は、区切りをまたがず1文字だけ
 * 進んで打ち切る (loadstr() の仕様)。
 */
int get_property(char **p, char *property, int psize, char *value, int vsize)
{
    if (p == nullptr || *p == nullptr) {
        return 0;
    }

    SKIP_SPACE(*p);
    if (**p == '\0' || **p == '\n') {
        return 0;
    }

    loadstr(property, p, '=', psize);
    if (**p == '"') {
        (*p)++;
        loadstr(value, p, '"', vsize);
    } else if (**p == '\'') {
        (*p)++;
        loadstr(value, p, '\'', vsize);
    } else {
        loadstr(value, p, ' ', vsize);
    }
    return 1;
}

/* ワイルドカードでプロパティチェック */
/* キー/値に ワイルドカード使用可     */
/* 合致 : -1   不適合 : 0             */
int match_property(char *pattern, char *data)
{
    std::array<char, MAXKEYLEN> pat_k{};
    std::array<char, MAXVALUELEN> pat_v{};
    std::array<char, MAXKEYLEN> ext_k{};
    std::array<char, MAXVALUELEN> ext_v{};

    /* パターンの各項目について、データ側に合う項目があるか調べる。 */
    while (get_property(&pattern, pat_k.data(), MAXKEYLEN,
                        pat_v.data(), MAXVALUELEN)) {
        char *ext = data;
        int found = 0;
        while (get_property(&ext, ext_k.data(), MAXKEYLEN,
                             ext_v.data(), MAXVALUELEN)) {
            /* KEY一致 */
            if (wildcmp(pat_k.data(), ext_k.data())) {
                /* 値不一致 */
                if (!match_oneproperty(pat_v.data(), ext_v.data())) {
                    return 0;
                }
                found = 1;
            }
        }
        /* キーが見つからず、パターンの値が空でない場合は不適合。
         * 値が空のパターン (key=) は、キーが無くても適合とする。 */
        if (!found && pat_v[0] != '\0') {
            return 0;
        }
    }
    return -1;
}

/* 数値/文字列 両用のパターンチェック */
/* 合致 : -1   不適合 : 0             */
int match_oneproperty(char *pat, char *val)
{
    /* パターン先頭の数字列を数値として読む。 */
    int i = 0;
    char *p = pat;
    while (*p >= '0' && *p <= '9') {
        i = i * 10 + (*p - '0');
        ++p;
    }
    /* パターンが数字だけ(または数字+v/^)で、値も数字で始まれば
     * 数値比較をする。v は「パターン >= 値」、^ は「パターン <= 値」、
     * 接尾辞なしは等しい場合に合致する。
     * *(p+1) の読みは *p != '\0' のときだけ評価される
     * (短絡評価)。*p が文字なら p+1 はその文字か終端であり、
     * 文字列の範囲内である。 */
    if (val[0] >= '0' && val[0] <= '9' &&
        (*p == '\0' ||
         (*(p + 1) == '\0' && (*p == 'v' || *p == '^')))) {
        /* %i は 0x/0 接頭辞を16進/8進として読む。値の数字の
         * 後ろに余分な文字があっても、先頭の数値だけを見る。 */
        int j = 0;
        std::sscanf(val, "%i", &j);
        if ((*p == '\0' && i == j) ||
            (*p == 'v' && i >= j) ||
            (*p == '^' && i <= j)) {
            return -1;
        }
        return 0;
    }
    return wildcmp(pat, val);
}

/* src のプロパティ文字列から、property に合う項目を除いて dst へ
 * 写し、末尾に property=value を追加する。value が NULL のときは
 * 追加せず、除くだけ(削除)になる。成功すれば 1、dst が一杯で
 * 写しきれなければ 0 を返す。
 * 合わない項目は原文のまま(空白や引用符も含めて)写す。項目の
 * 区切りには空白を1つ入れる。value に空白が含まれるときは
 * 引用符で囲み、値に " が含まれるときは ' で囲む。
 */
int set_property(char *src, char *dst, int dsize, char *property, char *value)
{
    std::array<char, MAXKEYLEN> dat_k{};
    std::array<char, MAXVALUELEN> dat_v{};
    char *bp = src;
    char *b2 = dst;

    while (dsize > 0 &&
           get_property(&src, dat_k.data(), MAXKEYLEN,
                        dat_v.data(), MAXVALUELEN)) {
        if (!wildcmp(property, dat_k.data())) {
            SKIP_SPACE(bp);
            for (; bp < src && dsize > 1; *dst++ = *bp++, --dsize) {
            }
            /* 区切りの空白を入れる。何も写していないときは
             * dst の1つ前を読まない (c-bugs 4-59)。 */
            if (dst != b2 && *(dst - 1) != ' ') {
                *dst++ = ' ';
                --dsize;
            }
        }
        bp = src;
    }

    if (dsize == 0) {
        return 0;
    }

    if (value == nullptr) {
        if (dst != b2) {
            --dst;
        }
        *dst = '\0';
    } else if (std::strstr(value, " ") != nullptr && value[0] != '"' &&
               value[0] != '\'') {
        if (std::strstr(value, "\"") == nullptr) {
            snprintf(dst, dsize, "%s=\"%s\"", property, value);
        } else {
            snprintf(dst, dsize, "%s='%s'", property, value);
        }
    } else {
        snprintf(dst, dsize, "%s=%s", property, value);
    }
    return 1;
}

/* プロパティ文字列 p から、property に一致するキーの値を探す。
 * 見つかれば value に写して 1、なければ 0 を返す。
 * キーの比較は完全一致で、ワイルドカードは使わない。
 * 同じキーが複数あれば、最初のものを使う。
 */
int get_propertyvalue(char *p, char *property, char *value, int vsize)
{
    std::array<char, MAXKEYLEN> ext_k{};
    std::array<char, MAXVALUELEN> ext_v{};

    while (get_property(&p, ext_k.data(), MAXKEYLEN,
                        ext_v.data(), MAXVALUELEN)) {
        if (std::strcmp(ext_k.data(), property) == 0) {
            strlcpy(value, ext_v.data(), vsize);
            return 1;
        }
    }
    return 0;
}
