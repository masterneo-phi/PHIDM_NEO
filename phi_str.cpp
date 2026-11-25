
/* made   2000/09/06  */
/* update 2002/03/24  */
/* Copyright by Qajin */

/* 文字列整形モジュール。dmprintf() はゲーム内メッセージ専用の
 * sprintf で、%name や %hp といったゲーム状態を差し込む書式を
 * 展開する。展開規則は下の対応表の通りで、テーブル駆動になっている。
 * 対応表の順序は意味を持つので (例: "ip" は "i" より先)、
 * 並べ替えるときは接頭辞の重なりを確認すること。
 */

#include  <stdio.h>
#include  <string.h>
#include  <stdarg.h>
#include  <sys/time.h>
#include  <time.h>

#include  <algorithm>
#include  <array>
#include  <cstring>
#include  <string>

#include  "phi_dm.h"
#include  "phi_data.h"
#include  "phi_str.h"
#include  "phi_class.h" /* PHIDM NEO extension: phi.jb job/gender */
#include  "tcp.h"

#ifdef DEBUG
char debug_buf[256];
#endif

/* dm用特殊sprintf                   */
/* %name   - n のキャラ名            */
/* %vsname - m   〃                  */
/* %Name   - n のキャラ名(31文字固定)*/

/*  vv nの有効時のみ可(無効時 0文字) */
/* %wpn    - 武器(非装備時 Knuckle)  */
/* %arm    - 鎧                      */
/* %acc    - アクセ                  */
/* %hp     - hp                      */
/* %mp     - mp                      */
/* %maxhp  - max hp                  */
/* %maxmp  - max mp                  */
/* %Hp     - hp(4桁固定)             */
/* %Mp     - mp(〃)                  */
/* %Maxhp  - max hp(〃)              */
/* %Maxmp  - max mp(〃)              */
/* %php    - "+hp"/"-hp"(hp<1/4max時)*/
/* %pmp    - "+mp"/"-mp"(mp<1/4max時)*/
/* %exp    - 経験値                  */
/* %gp     - 所持金                  */
/* %area   - 居るエリア              */
/* %flv    - Fighter  lv             */
/* %wlv    - Wizard   lv             */
/* %mlv    - Merchant lv             */
/* %clv    - Create   lv             */
/* %ip     - ip address              */
/* %domain - domain address          */
/* %talk   - 発言(スイッチ専用)      */
/* %sex    - 性別名 (phi.jb 職業・性別システム, PHIDM NEO) */
/* %job    - 職業名 (phi.jb 職業・性別システム, PHIDM NEO) */
/* %y?     - キャラ別変数?の値       */
/* %z?     - キャラ別保存変数?の値   */

/*  vv nによらず使用可               */
/* %dm     - DM(変更可)              */
/* %land   - 世界名                  */
/* %tax    - 国庫                    */
/* %cir    - 通貨単位                */
/* %ver    - Phi バージョン名        */
/* %%      - %                       */
/* %x??    - 汎用変数??の値          */
/* %\      - (無:次の文字との区切用) */

/*  vv 対応するパラメーターが必要    */
/* %i      - 数値                    */
/* %s      - 文字列                  */

namespace {

/* 書式展開の実行文脈。各 %書式 の処理関数に渡す。 */
struct FormatContext {
  int n;             /* 対象キャラ番号 */
  int m;             /* 相手キャラ番号 (%vsname 用) */
  va_list &args;     /* %i / %s の引数列 */
  char *&cursor;     /* 書式中の現在位置 (%x/%y/%z が16進数を読み進める) */
};

/* 数値を printf 書式で整形して std::string で返す。
 * 32文字の一時バッファを使うのは従来通り。 */
template <typename... Args>
std::string format_number(const char *fmt, Args... args) {
  char tmp[32];
  snprintf(tmp, sizeof(tmp), fmt, args...);
  return std::string(tmp);
}

std::string spec_name(FormatContext &ctx) {
  if (ctx.n > 0 && cd[ctx.n].name != NULL) return cd[ctx.n].name;
  return std::string();
}

std::string spec_dm(FormatContext &) {
  return DMM_DM;
}

std::string spec_ip(FormatContext &ctx) {
  if (ctx.n > 0 && ctx.n < MAX_NTTYS) return ip_address_check(ctx.n);
  return std::string();
}

std::string spec_int(FormatContext &ctx) {
  return format_number("%i", va_arg(ctx.args, int));
}

std::string spec_string(FormatContext &ctx) {
  const char *s = va_arg(ctx.args, char *);
  /* 従来は NULL を渡すと落ちていた。空文字として扱う。 */
  return s != NULL ? std::string(s) : std::string();
}

std::string spec_gvar(FormatContext &ctx) {
  const int i = hexadecimal_load_arbitrarily(&ctx.cursor);
  if (i >= 0 && i < GVAR_MAX)
    return format_number("%i", generic_variable[i]);
  return std::string();
}

std::string spec_pvar(FormatContext &ctx) {
  const int i = hexadecimal_load_arbitrarily(&ctx.cursor);
  if (i >= 0 && i < PVAR_MAX)
    return format_number("%i", cd[ctx.n].pvar[i]);
  return std::string();
}

std::string spec_svar(FormatContext &ctx) {
  const int i = hexadecimal_load_arbitrarily(&ctx.cursor);
  if (i >= 0 && i < SVAR_MAX)
    return format_number("%i", cd[ctx.n].svar[i]);
  return std::string();
}

/* %\ は次の文字との区切りでしかない。何も出さない。 */
std::string spec_separator(FormatContext &) {
  return std::string();
}

std::string spec_padded_name(FormatContext &ctx) {
  if (ctx.n > 0 && cd[ctx.n].name != NULL)
    return format_number("%-31s", cd[ctx.n].name);
  return std::string();
}

std::string spec_weapon(FormatContext &ctx) {
  if (ctx.n > 0 && cd[ctx.n].type)
    return cd[ctx.n].weq != NULL ? cd[ctx.n].weq->name : DMM_KNUCKLE;
  return std::string();
}

std::string spec_armor(FormatContext &ctx) {
  if (ctx.n > 0 && cd[ctx.n].type && cd[ctx.n].aeq != NULL)
    return cd[ctx.n].aeq->name;
  return std::string();
}

std::string spec_accessory(FormatContext &ctx) {
  if (ctx.n > 0 && cd[ctx.n].type && cd[ctx.n].oeq != NULL)
    return cd[ctx.n].oeq->name;
  return std::string();
}

std::string spec_maxhp(FormatContext &ctx) {
  if (ctx.n > 0 && cd[ctx.n].type)
    return format_number("%li", cd[ctx.n].mhp);
  return std::string();
}

std::string spec_maxmp(FormatContext &ctx) {
  if (ctx.n > 0 && cd[ctx.n].type)
    return format_number("%li", cd[ctx.n].mmp);
  return std::string();
}

std::string spec_hp(FormatContext &ctx) {
  if (ctx.n > 0 && cd[ctx.n].type)
    return format_number("%li", cd[ctx.n].hp);
  return std::string();
}

std::string spec_mp(FormatContext &ctx) {
  if (ctx.n > 0 && cd[ctx.n].type)
    return format_number("%li", cd[ctx.n].mp);
  return std::string();
}

std::string spec_padded_maxhp(FormatContext &ctx) {
  if (ctx.n > 0 && cd[ctx.n].type)
    return format_number("%4li", cd[ctx.n].mhp);
  return std::string();
}

std::string spec_padded_maxmp(FormatContext &ctx) {
  if (ctx.n > 0 && cd[ctx.n].type)
    return format_number("%4li", cd[ctx.n].mmp);
  return std::string();
}

std::string spec_padded_hp(FormatContext &ctx) {
  if (ctx.n > 0 && cd[ctx.n].type)
    return format_number("%4li", cd[ctx.n].hp);
  return std::string();
}

std::string spec_padded_mp(FormatContext &ctx) {
  if (ctx.n > 0 && cd[ctx.n].type)
    return format_number("%4li", cd[ctx.n].mp);
  return std::string();
}

std::string spec_php(FormatContext &ctx) {
  if (ctx.n > 0 && cd[ctx.n].type)
    return cd[ctx.n].hp < cd[ctx.n].mhp / 4 ? "-hp" : "+hp";
  return std::string();
}

std::string spec_pmp(FormatContext &ctx) {
  if (ctx.n > 0 && cd[ctx.n].type)
    return cd[ctx.n].mp < cd[ctx.n].mmp / 4 ? "-mp" : "+mp";
  return std::string();
}

std::string spec_vsname(FormatContext &ctx) {
  if (ctx.m > 0 && cd[ctx.m].name != NULL) return cd[ctx.m].name;
  return std::string();
}

std::string spec_percent(FormatContext &) {
  return "%";
}

std::string spec_currency(FormatContext &) {
  return cirunit;
}

std::string spec_version(FormatContext &) {
  return PHI_VERSION;
}

std::string spec_gp(FormatContext &ctx) {
  if (ctx.n > 0 && cd[ctx.n].type)
    return format_number("%-4i", cd[ctx.n].gp);
  return std::string();
}

std::string spec_exp(FormatContext &ctx) {
  if (ctx.n > 0 && cd[ctx.n].type)
    return format_number("%-8i", cd[ctx.n].exp);
  return std::string();
}

std::string spec_fighter_lv(FormatContext &ctx) {
  if (ctx.n > 0 && cd[ctx.n].type)
    return format_number("%4i", cd[ctx.n].lv[LV_FIGHTER]);
  return std::string();
}

std::string spec_wizard_lv(FormatContext &ctx) {
  if (ctx.n > 0 && cd[ctx.n].type)
    return format_number("%4i", cd[ctx.n].lv[LV_WIZARD]);
  return std::string();
}

std::string spec_merchant_lv(FormatContext &ctx) {
  if (ctx.n > 0 && cd[ctx.n].type)
    return format_number("%4i", cd[ctx.n].lv[LV_MERCHANT]);
  return std::string();
}

std::string spec_creator_lv(FormatContext &ctx) {
  if (ctx.n > 0 && cd[ctx.n].type)
    return format_number("%4i", cd[ctx.n].lv[LV_CREATER]);
  return std::string();
}

std::string spec_land(FormatContext &) {
  return land_name;
}

std::string spec_area(FormatContext &ctx) {
  if (ctx.n > 0 && cd[ctx.n].type) {
#ifndef AREA_SUB
    return fld[map[cd[ctx.n].y][cd[ctx.n].x].area].name;
#else
    return fld[map[cd[ctx.n].y][cd[ctx.n].x].area].name[map[cd[ctx.n].y][cd[ctx.n].x].area_sub];
#endif
  }
  return std::string();
}

std::string spec_tax(FormatContext &) {
  return format_number("%li", tax);
}

std::string spec_hextime(FormatContext &) {
  return format_number("%08lx", time(NULL) + tslide * 60 * 60);
}

std::string spec_talk(FormatContext &ctx) {
  if (ctx.n > 0 && cd[ctx.n].type) return cd[ctx.n].str;
  return std::string();
}

std::string spec_domain(FormatContext &ctx) {
  if (ctx.n > 0 && ctx.n < MAX_NTTYS) return ip_domain_check(ctx.n);
  return std::string();
}

std::string spec_sex(FormatContext &ctx) {
  if (ctx.n > 0 && cd[ctx.n].type)
    return jb_sex_name(jb_sex(ctx.n));
  return std::string();
}

std::string spec_job(FormatContext &ctx) {
  if (ctx.n > 0 && cd[ctx.n].type)
    return jb_job_name(jb_job(ctx.n));
  return std::string();
}

/* %書式の対応表。先頭から順に照合し、最初に一致したものを展開する。
 * 順序は従来の if-else の鎖と同じで、接頭辞の重なり
 * ("ip" と "i"、"maxhp" と "hp" など) を正しく扱うために必要。 */
struct FormatSpec {
  const char *keyword;
  size_t length;
  std::string (*handler)(FormatContext &);
};

#define SPEC(kw, fn) {kw, sizeof(kw) - 1, fn}

const FormatSpec kFormatSpecs[] = {
  SPEC("name", spec_name),
  SPEC("dm", spec_dm),
  SPEC("ip", spec_ip),
  SPEC("i", spec_int),
  SPEC("sex", spec_sex),
  SPEC("job", spec_job),
  SPEC("s", spec_string),
  SPEC("x", spec_gvar),
  SPEC("y", spec_pvar),
  SPEC("z", spec_svar),
  SPEC("\\", spec_separator),
  SPEC("Name", spec_padded_name),
  SPEC("wpn", spec_weapon),
  SPEC("arm", spec_armor),
  SPEC("acc", spec_accessory),
  SPEC("maxhp", spec_maxhp),
  SPEC("maxmp", spec_maxmp),
  SPEC("hp", spec_hp),
  SPEC("mp", spec_mp),
  SPEC("Maxhp", spec_padded_maxhp),
  SPEC("Maxmp", spec_padded_maxmp),
  SPEC("Hp", spec_padded_hp),
  SPEC("Mp", spec_padded_mp),
  SPEC("php", spec_php),
  SPEC("pmp", spec_pmp),
  SPEC("vsname", spec_vsname),
  SPEC("%", spec_percent),
  SPEC("cir", spec_currency),
  SPEC("ver", spec_version),
  SPEC("gp", spec_gp),
  SPEC("exp", spec_exp),
  SPEC("flv", spec_fighter_lv),
  SPEC("wlv", spec_wizard_lv),
  SPEC("mlv", spec_merchant_lv),
  SPEC("clv", spec_creator_lv),
  SPEC("land", spec_land),
  SPEC("area", spec_area),
  SPEC("tax", spec_tax),
  SPEC("hextime", spec_hextime),
  SPEC("talk", spec_talk),
  SPEC("domain", spec_domain),
};

#undef SPEC

}  /* namespace */

/* dmprintf: 書式 frm を展開して buf (長さ len) へ書く。
 * 展開結果が len-1 文字を超えた分は切り捨て、末尾に '\0' を置く。
 * 戻り値は書き込んだ文字列の末尾 (NUL の位置) を指す。
 * 引数が不正な場合は NULL を返す。
 * 対応表に無い %書式 は '%' だけが消え、直後の文字はそのまま出る。 */
char *dmprintf(int n, int m, char *buf, int len, char *frm, ...)
{
  /* n / m がキャラ数の範囲外だとハンドラが cd[] の外を引く (c-bugs 4-53) */
  if (buf == NULL || frm == NULL || n < 0 || m < 0 || len <= 0
      || n >= MAX_NTTYS + MAX_ENEMIES || m >= MAX_NTTYS + MAX_ENEMIES)
    return NULL;

  va_list argp;
  va_start(argp, frm);
  FormatContext ctx{n, m, argp, frm};

  std::string out;
  const size_t limit = static_cast<size_t>(len - 1);
  /* 従来は出力が埋まった時点で書式の走査をやめていた。
   * 同じ位置で走査を止める (可変引数の消費のされ方も同じ)。 */
  while (*frm != '\0' && out.size() < limit) {
    if (*frm == '%') {
      frm++;
      for (const FormatSpec &spec : kFormatSpecs) {
        if (std::strncmp(frm, spec.keyword, spec.length) == 0) {
          frm += spec.length;
          out += spec.handler(ctx);
          break;
        }
      }
      /* 一致なし: 何も出さず、次の文字を次の周回でそのまま写す。 */
    } else {
      out += *frm++;
    }
  }
  va_end(argp);

  const size_t copy_len = std::min(out.size(), limit);
  std::memcpy(buf, out.data(), copy_len);
  buf[copy_len] = '\0';
  return buf + copy_len;
}

/* dmprintf内蔵 check_write    */
void dmcheck_write(int n, char *frm)
{
    std::array<char, 256> buf{};

    dmprintf(n, 0, buf.data(), static_cast<int>(buf.size()), frm);
    check_write(n, buf.data());
}

/* コメントを考慮した文字列コピー         */
/* コメント途中で空き領域がなくなった時   */
/* コメントは取り消されるが               */
/* 後ろにはゴミが残る :-P                 */
/* コピー出来た文字数(非コメント分)が返る */
/* 仕組み: src を buf へ写しながら、コメント開始記号から終了記号 */
/* までをコメントとして数える。len はコメント外の文字数の上限で、 */
/* コメント中の文字は len を消費しない。コメントが閉じずに       */
/* 終わった場合は、コメント開始位置まで巻き戻して捨てる。 */
int cmtstrcpy(char *buf, int siz, char *src, int len)
{
    const int total = len;      /* 数える対象の文字数 (非コメント分) */
    int nest = 0;               /* コメントの深さ (0 = コメント外) */
    char *comment_start = buf;  /* 未完コメントの開始位置 (巻き戻し用) */
    char *dst = buf;
    const char *p = src;

    while (--siz > 0 && len > 0 && *p != '\0') {
        if (nest == 0 && p[0] == '/' && p[1] == '*') {
            comment_start = dst;
            nest++;
        } else if (nest > 0 && p[0] == '/' && p[-1] == '*') {
            nest--;
        } else if (nest == 0) {
            len--;
        }
        *dst++ = *p++;
    }
    if (nest > 0) {
        dst = comment_start;
    }
    *dst = '\0';
    return total - len;
}

/* pを(l-1)個のcで埋める */
void nstr(char *p, char c, int l)
{
    const int count = l - 1 > 0 ? l - 1 : 0;
    std::fill_n(p, count, c);
    p[count] = '\0';
}
