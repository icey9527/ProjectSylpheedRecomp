"use strict";
const $ = id => document.getElementById(id);
const labels = {done:"已验收", in_progress:"进行中", pending:"待完成", blocked:"受阻", unverified:"证据待核对"};
let latest = null, filter = "all", stageFilter = null, selectedPlan = null, loading = false;

function node(tag, className, text) {
  const element = document.createElement(tag);
  if (className) element.className = className;
  if (text !== undefined) element.textContent = text;
  return element;
}
function date(value) {
  if (!value) return "未记录";
  const d = new Date(value);
  return Number.isNaN(d.valueOf()) ? value : d.toLocaleString("zh-CN", {hour12:false});
}
function pill(status) { return node("span", `pill ${status}`, labels[status] || status); }
function empty(target, text) { target.replaceChildren(node("p", "empty", text)); }

function renderStages(state) {
  $("stages").replaceChildren(...state.stages.map((stage, index) => {
    const active = stage.id === state.current.stage_id;
    const box = node("button", `stage${active ? " active" : ""}${stageFilter === stage.id ? " selected" : ""}`);
    box.type = "button";
    box.setAttribute("aria-pressed", String(stageFilter === stage.id));
    const top = node("div", "stage-top");
    top.append(node("span", "stage-index", `PHASE ${String(index+1).padStart(2,"0")}`),
      pill(stage.completed === stage.total ? "done" : active ? "in_progress" : "pending"));
    const bottom = node("div", "stage-bottom");
    const bar = node("progress"); bar.max = stage.total; bar.value = stage.completed;
    bar.setAttribute("aria-label", `${stage.title} ${stage.completed}/${stage.total}`);
    bottom.append(bar, node("span", "", `${stage.completed} / ${stage.total}`));
    box.append(top, node("h3", "", stage.title), node("p", "", stage.description), bottom);
    box.addEventListener("click", () => {
      stageFilter = stageFilter === stage.id ? null : stage.id;
      renderStages(state); renderTasks(state);
      $("tasks").closest("section").scrollIntoView({behavior:"smooth",block:"start"});
    });
    return box;
  }));
}

function renderTasks(state) {
  const open = new Set([...$("tasks").querySelectorAll("details[open]")].map(d => d.dataset.id));
  const tasks = state.stages.flatMap(stage => stage.tasks.map(task => ({...task, stageTitle:stage.title})))
    .filter(task => (!stageFilter || task.stage_id === stageFilter) &&
      (filter === "all" || (filter === "done" ? task.status === "done" : task.status !== "done")));
  $("task-count").textContent = `${tasks.length} 项${stageFilter ? " · 已筛选阶段" : ""}`;
  $("all-stages").textContent = stageFilter ? "取消阶段筛选" : "全部阶段";
  $("tasks").replaceChildren(...tasks.map(task => {
    const details = node("details", "task"); details.dataset.id = task.id; details.open = open.has(task.id);
    const summary = node("summary");
    const symbol = task.status === "done" ? "✓" : task.status === "in_progress" ? "›" : task.status === "unverified" ? "!" : "·";
    summary.append(node("span", `task-mark ${task.status}`, symbol), node("span", "task-title", task.title),
      node("span", "task-stage", task.stageTitle), pill(task.status));
    const content = node("div", "task-detail"); content.append(node("p", "", task.note));
    if (task.verified_at) content.append(node("p", "", `验收记录日期：${task.verified_at}。路径存在只说明证据可读取，行为正确性仍需验证。`));
    if (!task.evidence.length) content.append(node("p", "", "尚无本项证据记录。"));
    for (const e of task.evidence) content.append(node("div", `evidence${e.present ? "" : " missing"}`,
      `${e.present ? "✓ 可读取" : "! 缺失"} · ${e.base === "repo" ? "仓库" : "工作区"}/${e.path}${e.present ? ` · ${date(e.modified_at)}` : ""}`));
    details.append(summary, content); return details;
  }));
  if (!tasks.length) empty($("tasks"), "这个筛选下没有验收项。");
}

function renderState(state) {
  if (!state) {
    $("percent").textContent = "—"; $("overall").value = 0;
    $("completion").textContent = "状态文件无效，暂停计算进度";
    $("headline").textContent = "请检查里程碑文件"; $("current").textContent = "";
    for (const id of ["stages","tasks","upcoming","versions"]) empty($(id), "状态不可读取，见上方错误。");
    return;
  }
  document.title = `${state.project} · 工程进度`;
  $("project").textContent = state.project; $("headline").textContent = state.headline;
  $("percent").textContent = String(state.summary.percent); $("overall").value = state.summary.percent;
  $("completion").textContent = `${state.summary.completed} / ${state.summary.total} 项已验收${state.summary.unverified ? ` · ${state.summary.unverified} 项证据待核对` : ""}`;
  $("scope").textContent = state.scope;
  $("current").textContent = `当前：${state.current.title} · ${labels[state.current.status]}`;
  $("source-meta").textContent = `来源 ${state.source} · 内容记录 ${state.updated_at} · 文件修改 ${date(state.modified_at)}`;
  renderStages(state); renderTasks(state);
  $("upcoming").replaceChildren(...state.upcoming.map((task,index) => {
    const row = node("div", "next"), text = node("div");
    text.append(node("h3", "", task.title), node("p", "", task.note));
    row.append(node("span", "next-number", String(index+1).padStart(2,"0")),text); return row;
  }));
  if (!state.upcoming.length) empty($("upcoming"), "当前计划中的下一步均已完成，请更新计划。");
  $("versions").replaceChildren(...state.versions.map(version => {
    const row = node("div", "version"), head = node("div", "version-head");
    head.append(node("strong", "", version.name), node("span", "", version.role));
    row.append(head, node("p", "", version.note)); return row;
  }));
}

function renderGit(git) {
  $("git-count").textContent = git.commit_count === null ? "Git 不可用" : `共 ${git.commit_count} 次提交`;
  $("git-meta").textContent = `${git.branch} · ${git.head ? git.head.slice(0,12) : "无法读取 HEAD"} · 显示最近 20 条`;
  $("tracking").textContent = git.ahead === null ? "本机 upstream 引用不可用；未联网查询远端。" :
    `本机记录：领先 ${git.ahead} / 落后 ${git.behind} · ${git.tracking_note}`;
  $("changes-title").textContent = git.errors.changes ? "工作树状态不可读取" : git.changes.length ? `工作树：${git.changes.length} 条未提交记录` : "工作树干净";
  $("changes").textContent = git.errors.changes || git.changes.join("\n") || "没有未提交更改。";
  const expanded = new Set([...$("commits").querySelectorAll("details[open]")].map(d => d.dataset.hash));
  $("commits").replaceChildren(...git.commits.map(commit => {
    const row = node("div", "commit"), meta = node("div", "commit-meta");
    meta.append(node("span", "commit-hash", commit.short), node("span", "", date(commit.date)));
    row.append(meta,node("p", "commit-title", commit.subject));
    if (commit.body) {
      const more = node("details", "commit-body"); more.dataset.hash = commit.hash; more.open = expanded.has(commit.hash);
      more.append(node("summary", "", "提交说明"), node("pre", "", commit.body)); row.append(more);
    }
    return row;
  }));
  if (!git.commits.length) empty($("commits"), "尚无可读取的提交记录。");
}

function renderPlan() {
  const plans = latest?.notes.plans || [];
  const plan = plans.find(p => p.name === selectedPlan) || plans[0];
  selectedPlan = plan?.name || null;
  $("plan-select").value = selectedPlan || "";
  $("plan-date").textContent = plan ? `文件修改 ${date(plan.modified_at)}${plan.truncated ? " · 显示前 16,000 字符" : ""}` : "plans/ 中没有可读取的计划。";
  $("plan-content").textContent = plan?.text || "计划位于仓库外层 plans/，新增或修改后自动更新。";
}
function renderNotes(notes) {
  $("plan-select").replaceChildren(...notes.plans.map(plan => {
    const option = node("option", "", plan.name); option.value = plan.name; return option;
  }));
  renderPlan();
  $("handoff-date").textContent = notes.handoff ? `文件修改 ${date(notes.handoff.modified_at)}${notes.handoff.truncated ? " · 显示最后 16,000 字符" : ""}` : "尚无 HANDOFF.md";
  $("handoff-content").textContent = notes.handoff?.text || "交接记录保存在本机工作区根目录。";
}

async function refresh() {
  if (loading) return;
  loading = true; $("refresh").disabled = true;
  try {
    const response = await fetch("/api/status", {cache:"no-store", signal:AbortSignal.timeout(12000)});
    if (!response.ok) throw new Error(`HTTP ${response.status}`);
    const data = await response.json(); latest = data;
    const errors = [...data.errors,...data.notes.errors];
    if (!data.git.available) errors.push("Git 不可用，请检查安装或仓库目录。");
    $("errors").hidden = !errors.length; $("errors").textContent = errors.join("\n");
    $("connection").textContent = errors.length ? "已读取 · 有待处理项" : "实时读取";
    $("connection").classList.toggle("offline", errors.length > 0);
    renderState(data.state); renderGit(data.git); renderNotes(data.notes);
    $("observed").textContent = `最近读取 ${date(data.observed_at)} · 每 5 秒刷新`;
  } catch(error) {
    $("connection").textContent = "连接中断"; $("connection").classList.add("offline");
    $("errors").hidden = false;
    $("errors").textContent = `无法读取本地服务器：${error.message}。当前显示的是上次读取结果；请确认启动窗口仍在运行。`;
  } finally { loading = false; $("refresh").disabled = false; }
}
$("refresh").addEventListener("click", refresh);
$("plan-select").addEventListener("change", () => {selectedPlan = $("plan-select").value; renderPlan();});
$("all-stages").addEventListener("click", () => {stageFilter = null; if(latest?.state) {renderStages(latest.state); renderTasks(latest.state);}});
document.querySelectorAll("[data-filter]").forEach(button => button.addEventListener("click", () => {
  filter = button.dataset.filter;
  document.querySelectorAll("[data-filter]").forEach(b => b.setAttribute("aria-pressed",String(b === button)));
  if(latest?.state) renderTasks(latest.state);
}));
refresh(); setInterval(refresh, 5000);
