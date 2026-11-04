"""Build docs/agentic-engineering-feature-matrix.html from docs/feature-plan.md.

feature-plan.md is the only source: every feature's competitor scores, its Attome AE score and
status, the plan, today's evidence and how the leaders do it. This script checks the honesty
rules, refreshes the summary table inside feature-plan.md, and writes the HTML page.

Usage: python docs/tools/build_feature_plan.py
"""
import html
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MD = ROOT / "feature-plan.md"
OUT = ROOT / "agentic-engineering-feature-matrix.html"
CSS = Path(__file__).resolve().parent / "feature-plan.css"
COMP = ["Fusion", "nTop", "Siemens", "Creo", "Ansys", "SimScale", "Neural Concept", "PhysicsX", "Synera", "Zoo", "Adam", "PIL paper", "HEPH paper"]
NC = len(COMP)
PRODUCT = "Attome AE"
STATUSES = ["Designed", "Promised", "Carried over", "Missing"]


def band(v):
    return "b0" if v == 0 else "b1" if v <= 30 else "b2" if v <= 60 else "b3" if v <= 80 else "b4"


def inline(t):
    """Markdown inline subset: `code`, **bold**, *em*. Code spans are protected first so a `*` inside
    them (for example `base-*`) is never read as emphasis."""
    codes = []

    def keep(m):
        codes.append(m.group(1))
        return f"\x00{len(codes) - 1}\x00"

    t = re.sub(r"`([^`]+)`", keep, t)
    t = html.escape(t, quote=False)
    t = re.sub(r"\*\*([^*]+)\*\*", r"<strong>\1</strong>", t)
    t = re.sub(r"(?<![*\w])\*([^*]+)\*(?!\*)", r"<em>\1</em>", t)
    return re.sub(r"\x00(\d+)\x00", lambda m: f"<code>{html.escape(codes[int(m.group(1))], quote=False)}</code>", t)


def block(t):
    """A plan entry: paragraphs, plus '- ' lines rendered as list items."""
    items, para = [], []
    for ln in t.splitlines():
        ln = ln.strip()
        if ln.startswith("- "):
            if para:
                items.append(" ".join(para)); para = []
            items.append(ln[2:])
        elif ln:
            para.append(ln)
    if para:
        items.append(" ".join(para))
    return "".join(f"<li>{inline(x)}</li>" for x in items)


def slug(t):
    return re.sub(r"-+", "-", re.sub(r"[^a-z0-9]+", "-", t.lower())).strip("-")


# ---------------------------------------------------------------- parse
text = MD.read_text(encoding="utf-8")
cats = []
errors = []
for cm in re.finditer(r"^## ([A-Z]+)\. (.+?)\n(.*?)(?=^## [A-Z]+\. |\Z)", text, re.M | re.S):
    letter, title, body = cm.group(1), cm.group(2).strip(), cm.group(3)
    first = body.find("\n### ")
    blurb = body[: first if first >= 0 else len(body)].strip()
    feats = []
    for fm in re.finditer(r"^### ([A-Z]+)(\d+)\. (.+?)\n(.*?)(?=^### |\Z)", body, re.M | re.S):
        name, fb = fm.group(3).strip(), fm.group(4)
        where = f"{letter}{fm.group(2)} {name}"
        m = re.search(r"\*\*Attome AE:\*\* (\d+) · (Designed|Promised|Carried over|Missing)", fb)
        c = re.search(r"\*\*Competitors:\*\* (.+)", fb)
        p = re.search(r"\*\*Plan:\*\* (.+?)(?=\n\n\*\*|\Z)", fb, re.S)
        t = re.search(r"\*\*Today \(score (\d+)\):\*\*\n(.*?)(?=\n\*\*|\Z)", fb, re.S)
        lead_m = re.search(r"\*\*How the leaders do it:\*\*\n(.*)", fb, re.S)
        if not (m and c and p and t):
            errors.append(f"{where}: missing an Attome AE, Competitors, Plan or Today line")
            continue
        score, status, today = int(m.group(1)), m.group(2), int(t.group(1))
        comps, lowc = [], []
        for cname in COMP:
            cv = re.search(r"(?:^|· )" + re.escape(cname) + r" (\d+)(†?)", c.group(1))
            comps.append(int(cv.group(1)) if cv else None)
            lowc.append(bool(cv and cv.group(2)))
        if None in comps:
            errors.append(f"{where}: a competitor score is missing")
            continue
        # honesty rules
        if status in ("Promised", "Carried over") and score != today:
            errors.append(f"{where}: {status} must keep today's score {today}, has {score}")
        if status == "Missing" and score != 0:
            errors.append(f"{where}: Missing must score 0")
        if status == "Carried over" and today == 0:
            errors.append(f"{where}: Carried over needs something today (today is 0); use Missing")
        listify = lambda blk: [ln[2:].strip() for ln in blk.strip().splitlines() if ln.startswith("- ")]
        feats.append(dict(
            id=f"{letter.lower()}-{slug(name)}", name=name, score=score, status=status, today=today,
            comps=comps, lowc=[False] + lowc, plan=p.group(1).strip(),
            today_items=listify(t.group(2)), leader_items=listify(lead_m.group(1)) if lead_m else []))
    cats.append(dict(letter=letter, title=title, blurb=blurb, feats=feats))

if errors:
    print("\n".join(errors))
    sys.exit(f"{len(errors)} problems in feature-plan.md")


# ---------------------------------------------------------------- compute
def mean(xs):
    return round(sum(xs) / len(xs)) if xs else 0


def rel(f):
    lead = max(f["comps"])
    return "beat" if f["score"] > lead else "tie" if f["score"] == lead else "trail"


allf = [f for c in cats for f in c["feats"]]
n = len(allf)
by_status = {s: sum(1 for f in allf if f["status"] == s) for s in STATUSES}
by_rel = {r: sum(1 for f in allf if rel(f) == r) for r in ("beat", "tie", "trail")}
overall = [mean([f["score"] for f in allf])] + [mean([f["comps"][i] for f in allf]) for i in range(NC)]
wins = [sum(1 for f in allf if f["score"] >= max(f["comps"]))] + [
    sum(1 for f in allf if f["comps"][i] >= max([f["score"]] + f["comps"])) for i in range(NC)]
names = ["Attome AE"] + COMP


def cat_means(c):
    fs = c["feats"]
    return [mean([f["score"] for f in fs])] + [mean([f["comps"][i] for f in fs]) for i in range(NC)]


# ---------------------------------------------------------------- summary inside the markdown
rows = ["| Category | Features | Designed | Attome AE | Best competitor |", "|---|---|---|---|---|"]
for c in cats:
    ms = cat_means(c)
    best = max(ms[1:])
    who = ", ".join(nm for nm, v in zip(COMP, ms[1:]) if v == best)
    rows.append(f"| {c['letter']} {c['title']} | {len(c['feats'])} | {sum(1 for f in c['feats'] if f['status'] == 'Designed')} | "
                f"{ms[0]} | {best} ({who}) |")
best_all = max(overall[1:])
rows.append(f"| **All** | **{n}** | **{by_status['Designed']}** | **{overall[0]}** | **{best_all}** |")
summary = ("<!-- summary:start -->\n"
           f"**Scoreboard (generated):** Attome AE **{overall[0]}** · "
           + " · ".join(f"{nm} {v}" for nm, v in zip(COMP, overall[1:]))
           + f". Designed {by_status['Designed']} · Promised {by_status['Promised']} · Carried over "
           f"{by_status['Carried over']} · Missing {by_status['Missing']}. Attome AE beats the best competitor on "
           f"{by_rel['beat']} features, ties {by_rel['tie']}, trails {by_rel['trail']}.\n\n"
           + "\n".join(rows) + "\n<!-- summary:end -->")
new_text = re.sub(r"<!-- summary:start -->.*?<!-- summary:end -->", summary, text, count=1, flags=re.S)
if new_text != text:
    MD.write_text(new_text, encoding="utf-8", newline="\n")


# ---------------------------------------------------------------- html
def tds(vals, low=None):
    top = max(vals)
    low = low or [False] * len(vals)
    return "".join(f'<td class="{band(v)}{" att" if i == 0 else ""}{" top" if v == top else ""}"'
                   f'{' title="low confidence"' if low[i] else ""}><span>{v}{"<sup>†</sup>" if low[i] else ""}</span></td>'
                   for i, v in enumerate(vals))


def best_names(vals):
    top = max(vals)
    return ", ".join(nm for nm, v in zip(names, vals) if v == top)


head_cells = "".join(f'<th class="{"att" if i == 0 else ""}">{nm}</th>' for i, nm in enumerate(names))
board = []
for c in cats:
    ms = cat_means(c)
    board.append(f'<tr><th class="feat" scope="row"><a href="#cat-{c["letter"].lower()}"><span class="letter">{c["letter"]}</span>'
                 f'{html.escape(c["title"])}</a></th><td class="num">{len(c["feats"])}</td>'
                 f'<td class="num">{sum(1 for f in c["feats"] if f["status"] == "Designed")}</td>{tds(ms)}<td class="best">{best_names(ms)}</td></tr>')
board.append(f'<tr class="total"><th class="feat" scope="row">All features</th><td class="num">{n}</td>'
             f'<td class="num">{by_status["Designed"]}</td>{tds(overall)}<td class="best">{best_names(overall)}</td></tr>')
board.append('<tr class="wins"><th class="feat" scope="row">Features won (ties count for each)</th><td></td><td></td>'
             + "".join(f'<td class="{"att" if i == 0 else ""}"><span>{w}</span></td>' for i, w in enumerate(wins)) + "<td></td></tr>")
board_html = ('<div class="tbl-wrap"><table class="scores board"><thead><tr><th class="feat">Category</th><th class="num">Features</th>'
              f'<th class="num">Designed</th>{head_cells}<th class="best">Best</th></tr></thead><tbody>{"".join(board)}</tbody></table></div>')

STATUS_CLASS = {"Designed": "st-designed", "Promised": "st-promised", "Carried over": "st-carried", "Missing": "st-missing"}
sections = []
for c in cats:
    trs, hows = [], []
    for f in c["feats"]:
        vals = [f["score"]] + f["comps"]
        key = slug(f["status"])
        attrs = f'data-status="{key}" data-rel="{rel(f)}" data-name="{html.escape(f["name"].lower())}"'
        trs.append(f'<tr {attrs}><th class="feat" scope="row"><a href="#{f["id"]}">{html.escape(f["name"])}</a>'
                   f' <span class="st {STATUS_CLASS[f["status"]]}">{f["status"]}</span></th>{tds(vals, f["lowc"])}<td class="best">{best_names(vals)}</td></tr>')
        lead = max(f["comps"])
        verdict = {"beat": "beats the leader", "tie": "ties the leader", "trail": "trails the leader"}[rel(f)]
        hows.append(
            f'<details class="how" id="{f["id"]}" {attrs}><summary><span class="pill {band(f["score"])}">{f["score"]}</span>'
            f'{html.escape(f["name"])} <span class="st {STATUS_CLASS[f["status"]]}">{f["status"]}</span></summary><div class="how-body">'
            f'<div class="side plan"><h4>Attome AE plan · {f["score"]}, {verdict} ({lead})</h4><ul>{block(f["plan"])}</ul></div>'
            f'<div class="side"><h4>Today · {f["today"]}</h4><ul>{"".join(f"<li>{inline(x)}</li>" for x in f["today_items"])}</ul></div>'
            f'<div class="side"><h4>How the leaders do it</h4><ul>{"".join(f"<li>{inline(x)}</li>" for x in f["leader_items"])}</ul></div>'
            '</div></details>')
    sections.append(
        f'<section class="cat" id="cat-{c["letter"].lower()}" data-cat><h2><span class="letter">{c["letter"]}</span>{html.escape(c["title"])} '
        f'<small>{len(c["feats"])} features</small></h2><p class="blurb">{inline(c["blurb"])}</p><div class="tbl-wrap"><table class="scores">'
        f'<thead><tr><th class="feat">Feature</th>{head_cells}<th class="best">Best</th></tr></thead><tbody>{"".join(trs)}</tbody></table></div>'
        f'<div class="hows">{"".join(hows)}</div></section>')


def worklist(status, title, blurb):
    parts = []
    for c in cats:
        fs = [f for f in c["feats"] if f["status"] == status]
        if not fs:
            continue
        lis = "".join(f'<li><a href="#{f["id"]}">{html.escape(f["name"])}</a> <span class="pill {band(f["score"])}">{f["score"]}</span>'
                      f' vs {max(f["comps"])}</li>' for f in fs)
        parts.append(f'<h3><span class="letter">{c["letter"]}</span> {html.escape(c["title"])} ({len(fs)})</h3><ul class="cols">{lis}</ul>')
    return f'<section id="{slug(status)}-list"><h2>{title} ({by_status[status]})</h2><p class="blurb">{blurb}</p>{"".join(parts)}</section>'


toc = ('<a href="#scoreboard">Scoreboard</a>'
       + "".join(f'<a href="#cat-{c["letter"].lower()}"><span class="letter">{c["letter"]}</span>{html.escape(c["title"])}</a>' for c in cats)
       + '<a href="#promised-list">Promised</a><a href="#missing-list">Missing</a>')
seg = "".join(
    f'<button type="button" data-k="{k}" data-v="{v}" aria-pressed="{"true" if k == "all" else "false"}">{label} <b>{cnt}</b></button>'
    for k, v, label, cnt in [
        ("all", "", "All", n),
        ("status", "designed", "Designed", by_status["Designed"]),
        ("status", "promised", "Promised", by_status["Promised"]),
        ("status", "carried-over", "Carried over", by_status["Carried over"]),
        ("status", "missing", "Missing", by_status["Missing"]),
        ("rel", "beat", "Beats leader", by_rel["beat"]),
        ("rel", "trail", "Trails", by_rel["trail"]),
    ])
ranked = sorted(zip(COMP, overall[1:]), key=lambda x: -x[1])
extra_css = """
.st { font-size: 0.72em; font-weight: 600; padding: 1px 6px; border-radius: 999px; margin-left: 6px; white-space: nowrap; vertical-align: middle; border: 1px solid var(--line); color: var(--muted); }
.st-designed { color: var(--accent); border-color: var(--accent); }
.st-promised { border-style: dashed; }
.st-missing { opacity: 0.7; }
.how-body .side.plan h4 { color: var(--accent); }
"""
page = f"""<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Attome AE Feature Matrix</title>
<style>
{CSS.read_text(encoding="utf-8")}{extra_css}</style>
</head>
<body>
<div class="page">
<header class="masthead">
  <p class="eyebrow">Attome Agentic Engineering · feature plan · generated from feature-plan.md</p>
  <h1>Attome AE vs CAD, simulation and AI-engineering tools</h1>
  <p class="dek">{n} features. Attome AE scores <strong>{overall[0]}</strong> from the plan as written (""" + ", ".join(f"{c} {v}" for c, v in ranked) + f"""). The plan designs {by_status["Designed"]} features, promises {by_status["Promised"]}, carries {by_status["Carried over"]} over from today, and is missing {by_status["Missing"]}. Attome AE beats the best competitor on {by_rel["beat"]}, ties {by_rel["tie"]}, trails {by_rel["trail"]}.</p>
  <div class="legend" aria-label="Score bands"><span class="chip b0">0 absent</span><span class="chip b1">1–30 stub</span><span class="chip b2">31–60 partial</span><span class="chip b3">61–80 solid</span><span class="chip b4">81–100 leading</span><span class="chip top-demo">ring = best in row</span></div>
</header>
<nav class="toc" aria-label="Sections">{toc}</nav>
<main>
<section id="how-to-read">
<h2>How to read this</h2>
<ul>
<li><strong>Source:</strong> everything on this page comes from <code>docs/feature-plan.md</code>. Edit the markdown, then run <code>python docs/tools/build_feature_plan.py</code>.</li>
<li><strong>Attome AE score, by status:</strong> <em>Designed</em> — the plan explains how it is built, so it gets the plan's score. <em>Promised</em> — a target without a design keeps today's score. <em>Carried over</em> — exists today and the plan does not change it yet, so it keeps today's score. <em>Missing</em> — nothing today and nothing planned, 0.</li>
<li>Each feature opens to the plan, what Attome AE has today, and how the leaders do it. Rings mark the best score in each row.</li>
<li>Competitor scores are analyst judgement from the cited November 2026 research, for shipped capability; a † marks low confidence; detail rows carry the score of the researched row they refine. Treat ±5 as noise.</li>
</ul>
</section>
<section id="scoreboard"><h2>Scoreboard</h2><p class="blurb">Unweighted mean of the feature scores in each category.</p>{board_html}</section>
<div class="filters" role="group" aria-label="Filter features">
  <label class="search"><span>Find a feature</span><input id="q" type="search" placeholder="e.g. SAML, forecasting, rollup" autocomplete="off"></label>
  <div class="seg" id="seg">{seg}</div>
  <button type="button" id="toggle-how" aria-pressed="false">Expand all explanations</button>
</div>
{"".join(sections)}
{worklist("Promised", "Promised but not designed", "Targets the plan names without saying how to build them. Designing these is the fastest way to raise the score.")}
{worklist("Missing", "Missing", "Nothing today and nothing in the plan. Each needs a plan entry, or an explicit decision not to build it.")}
</main>
<footer><p>Generated from <code>docs/feature-plan.md</code>. Competitor scores: analyst judgement from the cited research of November 2026 (docs/research/competitive-landscape-2026-11.md); † marks low confidence.</p></footer>
</div>
<script>
(function(){{
  var q=document.getElementById('q'), seg=document.getElementById('seg'), k='all', v='';
  function apply(){{
    var term=(q.value||'').trim().toLowerCase();
    document.querySelectorAll('section[data-cat]').forEach(function(sec){{
      var shown=0;
      sec.querySelectorAll('tr[data-status], details.how').forEach(function(el){{
        var ok=(k==='all'||el.dataset[k]===v)&&(!term||el.dataset.name.indexOf(term)>-1);
        el.hidden=!ok; if(ok&&el.tagName==='TR')shown++;
      }});
      sec.hidden=shown===0;
    }});
  }}
  q.addEventListener('input',apply);
  seg.addEventListener('click',function(e){{
    var b=e.target.closest('button'); if(!b)return;
    k=b.dataset.k; v=b.dataset.v;
    seg.querySelectorAll('button').forEach(function(x){{x.setAttribute('aria-pressed',x===b?'true':'false');}});
    apply();
  }});
  var t=document.getElementById('toggle-how');
  t.addEventListener('click',function(){{
    var open=t.getAttribute('aria-pressed')!=='true';
    document.querySelectorAll('details.how').forEach(function(d){{d.open=open;}});
    t.setAttribute('aria-pressed',open?'true':'false');
    t.textContent=open?'Collapse all explanations':'Expand all explanations';
  }});
  function openHash(h){{var el=document.getElementById(h);if(el&&el.tagName==='DETAILS'){{el.open=true;}}}}
  document.addEventListener('click',function(e){{
    var a=e.target.closest('a[href^="#"]'); if(a)openHash(a.getAttribute('href').slice(1));
  }});
  if(location.hash)openHash(location.hash.slice(1));
}})();
</script>
</body>
</html>
"""
OUT.write_text(page, encoding="utf-8", newline="\n")
print(f"{n} features · Attome AE {overall[0]} · " + ", ".join(f"{c} {v}" for c, v in zip(COMP, overall[1:])))
print(f"status {by_status} · vs leader {by_rel} · wins {dict(zip(names, wins))}")
