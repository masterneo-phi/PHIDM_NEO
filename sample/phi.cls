-- phi.cls サンプルライブラリ: クエスト構築支援 (PHIDM NEO)
--
-- このファイルはライブラリです。世界の phi.cls の先頭にこの内容を
-- 置き、その後ろに世界ごとのプログラム (動作 62 から呼ばれる関数)
-- を書いて使います。本体側の汎用フック (毎秒コールバック
-- on_second、dm.chain、dm.get_var / dm.set_var、文字列ストア
-- dm.get_str / dm.set_str / dm.del_str) の上に構築されています。
--
-- 提供するもの:
--   quest.*  クエストの進行管理 (文字列ストアの薄いラッパ)
--   ta.*     制限時間 (タイムアタック)。時間切れで :A チェーン発火
--
-- 使い方の例 (72 層型: 中央のマスに 12 分立ち続けると開く):
--   phi.cls (このライブラリの後ろに書く):
--     function ta72_start(ctx)
--       ta.start(ctx, 720, 7201, {step = 60, step_chain = 7202,
--                                 watch = {ctx.sx, ctx.sy},
--                                 fail_chain = 7203})
--     end
--     function ta72_left(ctx) ta.report(ctx) end
--     function ta72_stop(ctx) ta.stop(ctx) end
--   phi.swt:
--     :S 040 040 06 00 0000 040 040 62 "ta72_start"  (踏んで開始)
--     :A 7201 0000 0000 "封印が解けた。"
--     :A 7202 0000 0000 "炎が 1 つ灯った。"
--     :A 7203 0000 0000 "マスを離れたため、最初からやり直しだ。"
--
-- 使い方の例 (クエストの進行):
--     function q_talk(ctx)
--       if quest.stage(ctx, "main") == 0 then
--         dm.msg("まずは長老に会ってください。")
--       end
--     end
--     function q_elder(ctx)
--       quest.set_stage(ctx, "main", 1)
--       quest.set_flag(ctx, "met_elder", true)
--       dm.msg("長老の話を聞いた。")
--     end
--
-- 注意:
--   dm.* の各関数は「今このプログラムを起動したキャラ」に作用する
--   ので、quest.* / ta.* の ctx は呼び出し規約を揃えるための引数で、
--   中ではキーとしてキャラ名 (ctx.name) だけを使います。
--   文字列ストアの内容は phi.qst に保存され、再ログインやサーバ
--   再起動を跨いで残ります。dm.vars の内容は残りません。

----------------------------------------------------------------
-- quest: クエスト進行管理
--   フラグは "flag:<名前>"、段階は "stage:<名前>"、数え上げは
--   "count:<名前>" というキーで文字列ストアに保存されます。
--   それ以外のキーは quest.get / quest.set で自由に使えます。
----------------------------------------------------------------
quest = {}

function quest.get(ctx, key)
  return dm.get_str(key)
end

function quest.set(ctx, key, value)
  return dm.set_str(key, value)
end

function quest.del(ctx, key)
  return dm.del_str(key)
end

function quest.flag(ctx, name)
  return dm.get_str("flag:" .. name) == "1"
end

function quest.set_flag(ctx, name, on)
  return dm.set_str("flag:" .. name, on and "1" or "0")
end

function quest.stage(ctx, name)
  return tonumber(dm.get_str("stage:" .. name) or "") or 0
end

function quest.set_stage(ctx, name, n)
  return dm.set_str("stage:" .. name, tostring(n))
end

function quest.count(ctx, name)
  return tonumber(dm.get_str("count:" .. name) or "") or 0
end

function quest.add(ctx, name, d)
  local v = quest.count(ctx, name) + (d or 1)
  dm.set_str("count:" .. name, tostring(v))
  return v
end

----------------------------------------------------------------
-- ta: 制限時間 (タイムアタック)
--   ta.start(ctx, 秒数, 時間切れチェーン, opt)
--     opt.step / opt.step_chain : step 秒ごとに step_chain を発火
--     opt.watch = {x, y}        : そのマスを離れたら失敗
--     opt.fail_chain            : 離脱時に発火するチェーン
--     opt.persist = true        : 残り時間を文字列ストアに保存し、
--                                 再ログイン・再起動後も再開する
--                                 (watch の状態は保存されません)
--   ta.left(ctx)   残り秒数 (動いていなければ nil)
--   ta.report(ctx) 残り時間をメッセージで知らせる (動作 23 相当)
--   ta.stop(ctx)   無言で終了する (動作 24 相当)
--   ta.on_second(ctx) 毎秒の駆動部。世界の on_second から呼ぶか、
--                     未定義ならこのファイルが on_second として
--                     自動で定義します。
----------------------------------------------------------------
ta = {}

local function ta_live()
  dm.vars.ta = dm.vars.ta or {}
  return dm.vars.ta
end

local function ta_save(e)
  dm.set_str("ta:state", string.format("%d,%d,%d,%d",
             e.left, e.chain, e.step or 0, e.step_chain or 0))
end

function ta.start(ctx, sec, chain, opt)
  opt = opt or {}
  local e = {
    left = sec,
    chain = chain,
    step = opt.step,
    step_chain = opt.step_chain,
    fail_chain = opt.fail_chain,
    persist = opt.persist,
    elapsed = 0,
  }
  if opt.watch then
    e.watch_x = opt.watch[1]
    e.watch_y = opt.watch[2]
  end
  ta_live()[ctx.name] = e
  if e.persist then
    ta_save(e)
  else
    dm.del_str("ta:state") -- 古い保存状態が幽霊として復帰しないように
  end
end

function ta.left(ctx)
  local e = ta_live()[ctx.name]
  if e then
    return e.left
  end
  local s = dm.get_str("ta:state")
  if s then
    local left = s:match("^(%d+),")
    if left then
      return tonumber(left)
    end
  end
  return nil
end

function ta.report(ctx)
  local left = ta.left(ctx)
  if left then
    dm.msg(string.format("Time left: %d min %d sec",
                         math.floor(left / 60), left % 60))
  else
    dm.msg("No timer is running.")
  end
end

function ta.stop(ctx)
  ta_live()[ctx.name] = nil
  dm.del_str("ta:state")
end

function ta.on_second(ctx)
  local live = ta_live()
  local e = live[ctx.name]
  if not e then
    -- 保存されたタイマの復帰 (再ログイン / サーバ再起動後)
    local s = dm.get_str("ta:state")
    if not s then
      return
    end
    local left, chain, step, step_chain =
      s:match("^(%d+),(%d+),(%d+),(%d+)$")
    if not left then
      dm.del_str("ta:state")
      return
    end
    e = { left = tonumber(left), chain = tonumber(chain),
          elapsed = 0, persist = true }
    if tonumber(step) > 0 then
      e.step = tonumber(step)
      e.step_chain = tonumber(step_chain)
    end
    live[ctx.name] = e
  end
  if e.watch_x then
    local p = dm.player()
    if p and (p.x ~= e.watch_x or p.y ~= e.watch_y) then
      live[ctx.name] = nil
      if e.persist then
        dm.del_str("ta:state")
      end
      if e.fail_chain then
        dm.chain(e.fail_chain)
      end
      return
    end
  end
  e.elapsed = e.elapsed + 1
  e.left = e.left - 1
  if e.left <= 0 then
    live[ctx.name] = nil
    if e.persist then
      dm.del_str("ta:state")
    end
    dm.chain(e.chain)
    return
  end
  if e.step and e.step_chain and e.elapsed % e.step == 0 then
    dm.chain(e.step_chain)
  end
  if e.persist then
    ta_save(e)
  end
end

-- 世界が on_second を定義していない場合だけ、ta の駆動に使う
if on_second == nil then
  function on_second(ctx)
    ta.on_second(ctx)
  end
end
