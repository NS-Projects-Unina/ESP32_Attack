const state = {
    statusTimer: null,
    refreshMs: 2000
};

function text(id, value)
{
    document.getElementById(id).textContent = value;
}

async function requestJson(url, options = {})
{
    const response = await fetch(url, options);
    const data = await response.json();

    if (!response.ok)
    {
        throw new Error(data.message || "request failed");
    }

    return data;
}

function renderStatus(status)
{
    text("version", `v${status.version}`);
    text("scannerState", status.scanner_busy ? "Scanning" : "Idle");
    text("resultsState", status.scanner_ready ? "Ready" : "Waiting");
    text("scanCounter", status.scan_counter);
    text("stationState", status.station_status);
    text("captureState", status.capture_running ? "Running" : "Stopped");
    text("apSsid", status.ap_ssid || "-");
    text("apIp", status.ap_ip || "-");
    text("apClients", status.ap_clients !== undefined ? status.ap_clients : 0);
    text("natState", status.nat_routing ? "Enabled" : "Disabled");
    text("natDetails", status.nat_routing
        ? "freeWiFi clients can use the STA uplink."
        : status.ap_mode === "freewifi"
            ? "freeWiFi AP active without STA uplink."
            : "Connect STA to share connectivity.");

    const freewifiButton = document.getElementById("freewifiToggleButton");

    freewifiButton.dataset.active = status.ap_mode === "freewifi" ? "true" : "false";
    freewifiButton.textContent = status.ap_mode === "freewifi" ? "Disattiva AP" : "Attiva AP";
    freewifiButton.classList.toggle("secondary", status.ap_mode === "freewifi");
    freewifiButton.disabled = status.capture_running;

    const ssid = status.connected_ssid || "-";
    const ip = status.station_ip || "-";

    text("stationDetails", `SSID: ${ssid} | IP: ${ip}`);

    document.getElementById("disconnectButton").disabled =
        !status.connected && !status.connecting;
}

function formatBytes(bytes)
{
    if (bytes < 1024)
    {
        return `${bytes} B`;
    }

    return `${(bytes / 1024).toFixed(1)} KB`;
}

async function loadStatus()
{
    try
    {
        const status = await requestJson("/api/status");
        renderStatus(status);
        return status;
    }
    catch (err)
    {
        text("scannerState", "Offline");
        text("stationState", "Offline");
        text("captureState", "Offline");
        console.error(err);
        return null;
    }
}

async function refreshAll()
{
    await loadStatus();
    await loadResults();
    await loadCapture();
}

function updateRefreshTimer()
{
    if (state.statusTimer !== null)
    {
        clearInterval(state.statusTimer);
        state.statusTimer = null;
    }

    const enabled = document.getElementById("autoRefreshToggle").checked;

    if (enabled)
    {
        state.statusTimer = setInterval(refreshAll, state.refreshMs);
    }
}

function configureRefresh()
{
    const toggle = document.getElementById("autoRefreshToggle");
    const interval = document.getElementById("refreshIntervalInput");

    function apply()
    {
        const seconds = Math.min(10, Math.max(1, Number(interval.value) || 2));
        interval.value = seconds;
        state.refreshMs = seconds * 1000;
        updateRefreshTimer();
    }

    toggle.addEventListener("change", updateRefreshTimer);
    interval.addEventListener("change", apply);
    apply();
}

async function loadResults()
{
    try
    {
        const aps = await requestJson("/api/results");
        const table = document.getElementById("scanTable");

        table.innerHTML = "";

        aps.forEach(ap =>
        {
            const row = document.createElement("tr");
            const ssid = document.createElement("td");
            const rssi = document.createElement("td");
            const channel = document.createElement("td");
            const auth = document.createElement("td");

            ssid.textContent = ap.ssid || "(hidden)";
            rssi.textContent = ap.rssi;
            channel.textContent = ap.channel;
            auth.textContent = ap.auth;

            row.appendChild(ssid);
            row.appendChild(rssi);
            row.appendChild(channel);
            row.appendChild(auth);
            table.appendChild(row);
        });

        return aps.length;
    }
    catch (err)
    {
        console.error(err);
        return 0;
    }
}

async function loadCapture()
{
    try
    {
        const capture = await requestJson("/api/capture");

        text("captureState", capture.running ? "Running" : "Stopped");
        text("captureMode", capture.mode === "ap" ? "freeWiFi" : "All");
        text("captureTotal", capture.total);
        text("captureStored", capture.stored);
        text("captureDropped", capture.dropped);
        text("capturePcapBytes", formatBytes(capture.pcap_bytes));
        text("captureManagement", capture.management);
        text("captureControl", capture.control);
        text("captureData", capture.data);
        text("captureApTraffic", capture.ap_traffic);
        text("captureApUplink", capture.ap_uplink);
        text("captureApDownlink", capture.ap_downlink);
        text("captureApBytes", formatBytes(capture.ap_traffic_bytes));
        text("captureRssi", capture.total > 0 ? capture.last_rssi : "-");
        text("captureChannel", capture.channel || "-");
        text("captureDetails", capture.running
            ? `Listening ${capture.mode === "ap" ? "to freeWiFi traffic" : "on channel " + capture.channel}`
            : "Capture stopped");

        document.getElementById("captureStartButton").disabled = capture.running;
        document.getElementById("captureStopButton").disabled = !capture.running;

        const download = document.getElementById("captureDownloadLink");
        const hasPackets = capture.stored > 0;

        download.classList.toggle("disabled", !hasPackets);
        download.setAttribute("aria-disabled", hasPackets ? "false" : "true");
        download.tabIndex = hasPackets ? 0 : -1;
    }
    catch (err)
    {
        text("captureState", "Offline");
        console.error(err);
    }
}

async function startCapture()
{
    const channel = Number(document.getElementById("captureChannelInput").value) || 1;
    const mode = document.getElementById("captureModeInput").value;
    const button = document.getElementById("captureStartButton");

    button.disabled = true;

    try
    {
        await requestJson("/api/capture/start", {
            method: "POST",
            headers: {
                "Content-Type": "application/json"
            },
            body: JSON.stringify({ channel, mode })
        });

        await loadCapture();
        await loadStatus();

        const activeChannel =
            document.getElementById("captureChannel").textContent;

        if (mode !== "ap" && String(channel) !== activeChannel && activeChannel !== "-")
        {
            text("captureDetails",
                 `Requested channel ${channel}, using current WiFi channel ${activeChannel}`);
        }
    }
    catch (err)
    {
        text("captureState", err.message);
        console.error(err);
    }
}

async function stopCapture()
{
    const button = document.getElementById("captureStopButton");

    button.disabled = true;

    try
    {
        await requestJson("/api/capture/stop", {
            method: "POST"
        });

        await loadCapture();
        await loadStatus();
    }
    catch (err)
    {
        text("captureState", err.message);
        console.error(err);
    }
}

async function waitForScan()
{
    while (true)
    {
        const status = await loadStatus();

        if (status === null || status.scanner_ready)
        {
            await loadResults();
            break;
        }

        await new Promise(resolve => setTimeout(resolve, 500));
    }
}

async function scanWifi()
{
    const button = document.getElementById("scanButton");

    button.disabled = true;
    button.textContent = "Scanning";

    try
    {
        const result = await requestJson("/api/scan");

        if (result.status === "busy")
        {
            button.textContent = "Scanner Busy";
            await new Promise(resolve => setTimeout(resolve, 1000));
        }
        else
        {
            await waitForScan();
        }
    }
    catch (err)
    {
        console.error(err);
    }

    button.disabled = false;
    button.textContent = "Scan Networks";
}

async function connectStation(event)
{
    event.preventDefault();

    const button = document.getElementById("connectButton");
    const ssid = document.getElementById("ssidInput").value.trim();
    const password = document.getElementById("passwordInput").value;

    button.disabled = true;
    button.textContent = "Connecting";

    try
    {
        await requestJson("/api/connect", {
            method: "POST",
            headers: {
                "Content-Type": "application/json"
            },
            body: JSON.stringify({ ssid, password })
        });

        await loadStatus();
    }
    catch (err)
    {
        text("stationDetails", err.message);
        console.error(err);
    }

    button.disabled = false;
    button.textContent = "Connect";
}

async function disconnectStation()
{
    const button = document.getElementById("disconnectButton");

    button.disabled = true;

    try
    {
        await requestJson("/api/disconnect", {
            method: "POST"
        });

        await loadStatus();
    }
    catch (err)
    {
        text("stationDetails", err.message);
        console.error(err);
    }
}

async function toggleFreewifiAp()
{
    const button = document.getElementById("freewifiToggleButton");
    const active = button.dataset.active === "true";

    button.disabled = true;
    button.textContent = active ? "Disattivo" : "Attivo";

    try
    {
        await requestJson(active ? "/api/freewifi/stop" : "/api/freewifi/start", {
            method: "POST"
        });

        await loadStatus();
        await loadCapture();
    }
    catch (err)
    {
        text("natDetails", err.message);
        console.error(err);
    }
    finally
    {
        await loadStatus();
    }
}

window.onload = async function()
{
    configureRefresh();

    document
        .getElementById("scanButton")
        .addEventListener("click", scanWifi);

    document
        .getElementById("connectForm")
        .addEventListener("submit", connectStation);

    document
        .getElementById("disconnectButton")
        .addEventListener("click", disconnectStation);

    document
        .getElementById("captureStartButton")
        .addEventListener("click", startCapture);

    document
        .getElementById("captureStopButton")
        .addEventListener("click", stopCapture);

    document
        .getElementById("freewifiToggleButton")
        .addEventListener("click", toggleFreewifiAp);

    await refreshAll();
};
