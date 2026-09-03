(() => {
    "use strict";

    const DEFAULT_CAMERA = Object.freeze({
        yaw: 35,
        pitch: 15,
        zoom: 1,
        panX: 0,
        panY: 0,
        panZ: 0,
    });

    const camera = { ...DEFAULT_CAMERA };
    const DEFAULT_LIGHTING = Object.freeze({ preset: 0, exposure: 0 });
    const lighting = { ...DEFAULT_LIGHTING };
    const LIGHTING_LABELS = Object.freeze(["NEUTRAL", "BRIGHT", "DRAMATIC"]);
    const sources = new Map();
    const buttons = new Map();
    let previousTime = performance.now();
    let layoutFrame = 0;
    let outfitActive = true;
    let outfitWeight = 100;
    let outfitSource = "ARMOR-RECORD";

    const clamp = (value, minimum, maximum) => Math.min(maximum, Math.max(minimum, value));
    const wrapDegrees = (degrees) => ((degrees + 180) % 360 + 360) % 360 - 180;

    function activeActions() {
        return new Set(sources.values());
    }

    function updateButtonStates() {
        const active = activeActions();
        for (const [action, actionButtons] of buttons) {
            for (const button of actionButtons) {
                button.classList.toggle("is-active", active.has(action));
            }
        }
    }

    function startAction(source, action) {
        sources.set(source, action);
        updateButtonStates();
    }

    function stopAction(source) {
        if (sources.delete(source)) {
            updateButtonStates();
        }
    }

    function stopAllActions() {
        if (sources.size !== 0) {
            sources.clear();
            updateButtonStates();
        }
    }

    function sendCamera() {
        const payload = [
            camera.yaw,
            camera.pitch,
            camera.zoom,
            camera.panX,
            camera.panY,
            camera.panZ,
        ].map((value) => value.toFixed(5)).join(",");
        window.meridianNifSetCamera?.(payload);

        const yaw = Math.round(((camera.yaw % 360) + 360) % 360).toString().padStart(3, "0");
        const pitch = `${camera.pitch >= 0 ? "+" : "−"}${Math.abs(Math.round(camera.pitch)).toString().padStart(2, "0")}`;
        document.getElementById("cameraReadout").textContent =
            `Y ${yaw}° · P ${pitch}° · Z ${camera.zoom.toFixed(2)}`;
    }

    function sendLighting() {
        const payload = `${lighting.preset},${lighting.exposure.toFixed(1)}`;
        window.meridianNifSetLighting?.(payload);

        document.getElementById("lightingMode").textContent = LIGHTING_LABELS[lighting.preset];
        document.getElementById("exposureReadout").textContent =
            `${lighting.exposure >= 0 ? "+" : "−"}${Math.abs(lighting.exposure).toFixed(1)} EV`;
        for (const button of document.querySelectorAll("[data-lighting-preset]")) {
            const selected = Number(button.dataset.lightingPreset) === lighting.preset;
            button.classList.toggle("is-selected", selected);
            button.setAttribute("aria-pressed", selected ? "true" : "false");
        }
    }

    function advanceCamera(deltaSeconds) {
        const actions = activeActions();
        if (actions.size === 0) {
            return false;
        }

        const yawDirection = Number(actions.has("orbitRight")) - Number(actions.has("orbitLeft"));
        const pitchDirection = Number(actions.has("pitchUp")) - Number(actions.has("pitchDown"));
        const panXDirection = Number(actions.has("panRight")) - Number(actions.has("panLeft"));
        const panYDirection = Number(actions.has("panUp")) - Number(actions.has("panDown"));
        const zoomDirection = Number(actions.has("zoomOut")) - Number(actions.has("zoomIn"));

        camera.yaw = wrapDegrees(camera.yaw + yawDirection * 72 * deltaSeconds);
        camera.pitch = clamp(camera.pitch + pitchDirection * 52 * deltaSeconds, -80, 80);
        camera.panX = clamp(camera.panX + panXDirection * 0.7 * camera.zoom * deltaSeconds, -3, 3);
        camera.panY = clamp(camera.panY + panYDirection * 0.7 * camera.zoom * deltaSeconds, -3, 3);
        camera.zoom = clamp(camera.zoom * Math.exp(zoomDirection * 1.15 * deltaSeconds), 0.2, 5);
        return yawDirection !== 0 || pitchDirection !== 0 || panXDirection !== 0 ||
            panYDirection !== 0 || zoomDirection !== 0;
    }

    function animate(now) {
        const deltaSeconds = Math.min(Math.max((now - previousTime) / 1000, 0), 0.05);
        previousTime = now;
        if (advanceCamera(deltaSeconds)) {
            sendCamera();
        }
        requestAnimationFrame(animate);
    }

    function frameModel() {
        Object.assign(camera, DEFAULT_CAMERA);
        stopAllActions();
        sendCamera();
        window.meridianNifFrame?.("");
    }

    function closeTest() {
        stopAllActions();
        window.meridianNifClose?.("");
    }

    function requestModel(path) {
        const normalized = path.trim();
        const status = document.getElementById("modelStatus");
        if (normalized.length === 0) {
            status.textContent = "ENTER A RELATIVE .NIF PATH";
            return;
        }
        document.getElementById("modelPath").value = normalized;
        status.textContent = `REQUESTED ${normalized}`;
        setOutfitMode(false);
        window.meridianNifLoad?.(normalized);
    }

    function updateOutfitReadout() {
        if (outfitActive && outfitSource === "PLAYER-EQUIPPED") {
            document.getElementById("outfitReadout").textContent = "RUNTIME EQUIPPED";
            return;
        }
        if (outfitActive && outfitSource === "PLAYER-ACTOR") {
            document.getElementById("outfitReadout").textContent = "LIVE ACTOR FOUNDATION";
            return;
        }
        const slotButtons = [...document.querySelectorAll("[data-outfit-object]")]
            .filter((button) => !button.disabled);
        const visibleCount = slotButtons.filter(
            (button) => button.getAttribute("aria-pressed") === "true").length;
        document.getElementById("outfitReadout").textContent = outfitActive ?
            `${visibleCount} / ${slotButtons.length} VISIBLE` : "SINGLE MODEL";
    }

    function setOutfitMode(enabled) {
        outfitActive = enabled;
        for (const button of document.querySelectorAll("[data-outfit-object]")) {
            button.disabled = !enabled;
            if (enabled) {
                button.setAttribute("aria-pressed", "true");
            }
        }
        for (const button of document.querySelectorAll("[data-outfit-weight]")) {
            button.disabled = !enabled;
        }
        updateOutfitReadout();
    }

    function updateWeightReadout() {
        const labels = new Map([[0, "LEAN"], [50, "MID"], [100, "HEAVY"]]);
        document.getElementById("weightReadout").textContent =
            `${outfitWeight} / ${labels.get(outfitWeight) ?? "CUSTOM"}`;
        for (const button of document.querySelectorAll("[data-outfit-weight]")) {
            const selected = Number(button.dataset.outfitWeight) === outfitWeight;
            button.classList.toggle("is-selected", selected);
            button.setAttribute("aria-pressed", selected ? "true" : "false");
        }
    }

    function setOutfitWeight(weight) {
        if (!outfitActive) {
            return;
        }
        outfitWeight = clamp(weight, 0, 100);
        updateWeightReadout();
        document.getElementById("modelStatus").textContent =
            `REQUESTED ${outfitSource} HIDE OUTFIT · WT ${outfitWeight.toString().padStart(3, "0")}`;
        window.meridianNifSetWeight?.(outfitWeight.toString());
    }

    function requestOutfit(source) {
        outfitSource = source;
        setOutfitMode(true);
        updateWeightReadout();
        document.getElementById("modelStatus").textContent =
            `REQUESTED ${outfitSource} HIDE OUTFIT · WT ${outfitWeight.toString().padStart(3, "0")}`;
        if (outfitSource === "ARMOR-RECORD") {
            window.meridianNifLoadArmorOutfit?.("");
        } else if (outfitSource === "MODDED-RECORD") {
            window.meridianNifLoadModdedArmorOutfit?.("");
        } else if (outfitSource === "LAYERED-RECORD" ||
                   outfitSource === "TEXTURE-SWAP") {
            for (const button of document.querySelectorAll("[data-outfit-object]")) {
                const layeredPart = button.dataset.outfitObject === "101";
                button.disabled = !layeredPart;
                button.setAttribute("aria-pressed", layeredPart ? "true" : "false");
            }
            updateOutfitReadout();
            if (outfitSource === "TEXTURE-SWAP") {
                window.meridianNifLoadTextureSwapArmor?.("");
            } else {
                window.meridianNifLoadLayeredArmor?.("");
            }
        } else if (outfitSource === "PLAYER-EQUIPPED") {
            for (const button of document.querySelectorAll("[data-outfit-object]")) {
                button.disabled = true;
                button.setAttribute("aria-pressed", "false");
            }
            updateOutfitReadout();
            window.meridianNifLoadPlayerOutfit?.("");
        } else if (outfitSource === "PLAYER-ACTOR") {
            for (const button of document.querySelectorAll("[data-outfit-object], [data-outfit-weight]")) {
                button.disabled = true;
                button.setAttribute("aria-pressed", "false");
            }
            document.getElementById("weightReadout").textContent = "LIVE ACTOR";
            document.getElementById("modelStatus").textContent =
                "REQUESTED PLAYER-ACTOR EQUIPMENT + SKIN + HEAD";
            updateOutfitReadout();
            window.meridianNifLoadPlayerActor?.("");
        } else {
            window.meridianNifLoadOutfit?.("");
        }
    }

    function toggleOutfitObject(button) {
        if (!outfitActive) {
            return;
        }
        const visible = button.getAttribute("aria-pressed") !== "true";
        button.setAttribute("aria-pressed", visible ? "true" : "false");
        window.meridianNifSetObjectVisible?.(
            `${button.dataset.outfitObject},${visible ? 1 : 0}`);
        updateOutfitReadout();
    }

    function bindHoldButton(button) {
        const action = button.dataset.action;
        if (!buttons.has(action)) {
            buttons.set(action, []);
        }
        buttons.get(action).push(button);

        button.addEventListener("pointerdown", (event) => {
            if (event.button !== 0) {
                return;
            }
            event.preventDefault();
            button.setPointerCapture(event.pointerId);
            startAction(`pointer:${event.pointerId}`, action);
        });

        const releasePointer = (event) => stopAction(`pointer:${event.pointerId}`);
        button.addEventListener("pointerup", releasePointer);
        button.addEventListener("pointercancel", releasePointer);
        button.addEventListener("lostpointercapture", releasePointer);
        button.addEventListener("contextmenu", (event) => event.preventDefault());

        button.addEventListener("keydown", (event) => {
            if ((event.code === "Space" || event.code === "Enter") && !event.repeat) {
                event.preventDefault();
                startAction(`button:${event.code}`, action);
            }
        });
        button.addEventListener("keyup", (event) => {
            if (event.code === "Space" || event.code === "Enter") {
                event.preventDefault();
                stopAction(`button:${event.code}`);
            }
        });
    }

    function keyboardAction(event) {
        const key = event.key.toLowerCase();
        const horizontalLeft = key === "a" || event.key === "ArrowLeft";
        const horizontalRight = key === "d" || event.key === "ArrowRight";
        const verticalUp = key === "w" || event.key === "ArrowUp";
        const verticalDown = key === "s" || event.key === "ArrowDown";

        if (event.shiftKey) {
            if (horizontalLeft) return "panLeft";
            if (horizontalRight) return "panRight";
            if (verticalUp) return "panUp";
            if (verticalDown) return "panDown";
        } else {
            if (horizontalLeft) return "orbitLeft";
            if (horizontalRight) return "orbitRight";
            if (verticalUp) return "pitchUp";
            if (verticalDown) return "pitchDown";
        }

        if (key === "e" || event.key === "+" || event.key === "=") return "zoomIn";
        if (key === "q" || event.key === "-" || event.key === "_") return "zoomOut";
        return null;
    }

    function reportLayout() {
        layoutFrame = 0;
        if (typeof window.meridianNifLayout !== "function") {
            scheduleLayoutReport();
            return;
        }
        const rectangle = document.getElementById("previewViewport").getBoundingClientRect();
        const payload = [rectangle.left, rectangle.top, rectangle.width, rectangle.height]
            .map((value) => Math.round(value)).join(",");
        window.meridianNifLayout(payload);
    }

    function scheduleLayoutReport() {
        if (layoutFrame === 0) {
            layoutFrame = requestAnimationFrame(reportLayout);
        }
    }

    for (const button of document.querySelectorAll("[data-action]")) {
        bindHoldButton(button);
    }

    document.getElementById("frameModel").addEventListener("click", frameModel);
    document.getElementById("closeTest").addEventListener("click", closeTest);
    document.getElementById("modelLoader").addEventListener("submit", (event) => {
        event.preventDefault();
        requestModel(document.getElementById("modelPath").value);
    });
    for (const preset of document.querySelectorAll("[data-model-path]")) {
        preset.addEventListener("click", () => requestModel(preset.dataset.modelPath));
    }
    document.querySelector("[data-armor-outfit-preset]").addEventListener(
        "click", () => requestOutfit("ARMOR-RECORD"));
    document.querySelector("[data-modded-armor-outfit-preset]").addEventListener(
        "click", () => requestOutfit("MODDED-RECORD"));
    document.querySelector("[data-layered-armor-preset]").addEventListener(
        "click", () => requestOutfit("LAYERED-RECORD"));
    document.querySelector("[data-texture-swap-preset]").addEventListener(
        "click", () => requestOutfit("TEXTURE-SWAP"));
    document.querySelector("[data-player-outfit-preset]").addEventListener(
        "click", () => requestOutfit("PLAYER-EQUIPPED"));
    document.querySelector("[data-player-actor-preset]").addEventListener(
        "click", () => requestOutfit("PLAYER-ACTOR"));
    document.querySelector("[data-outfit-preset]").addEventListener(
        "click", () => requestOutfit("NIF-PATH"));
    for (const piece of document.querySelectorAll("[data-outfit-object]")) {
        piece.addEventListener("click", () => toggleOutfitObject(piece));
    }
    for (const weight of document.querySelectorAll("[data-outfit-weight]")) {
        weight.addEventListener("click", () =>
            setOutfitWeight(Number(weight.dataset.outfitWeight)));
    }
    for (const preset of document.querySelectorAll("[data-lighting-preset]")) {
        preset.addEventListener("click", () => {
            lighting.preset = Number(preset.dataset.lightingPreset);
            sendLighting();
        });
    }
    document.getElementById("exposure").addEventListener("input", (event) => {
        lighting.exposure = Number(event.target.value);
        sendLighting();
    });
    document.getElementById("resetLighting").addEventListener("click", () => {
        Object.assign(lighting, DEFAULT_LIGHTING);
        document.getElementById("exposure").value = lighting.exposure.toFixed(1);
        sendLighting();
    });
    window.addEventListener("keydown", (event) => {
        if (event.key === "Escape") {
            event.preventDefault();
            closeTest();
            return;
        }
        if (event.target instanceof HTMLInputElement ||
            event.target instanceof HTMLTextAreaElement ||
            event.target instanceof HTMLSelectElement ||
            event.target?.isContentEditable) {
            return;
        }
        if (event.key.toLowerCase() === "f" && !event.repeat) {
            event.preventDefault();
            frameModel();
            return;
        }
        const action = keyboardAction(event);
        if (action !== null) {
            event.preventDefault();
            if (!event.repeat) {
                startAction(`key:${event.code}`, action);
            }
        }
    });
    window.addEventListener("keyup", (event) => stopAction(`key:${event.code}`));
    window.addEventListener("blur", stopAllActions);
    window.addEventListener("pagehide", stopAllActions);
    document.addEventListener("visibilitychange", () => {
        if (document.hidden) {
            stopAllActions();
        }
    });
    window.addEventListener("resize", scheduleLayoutReport);
    new ResizeObserver(scheduleLayoutReport).observe(document.getElementById("previewViewport"));

    window.meridianNifApplyActorState = (resolvedWeight) => {
        if (outfitSource !== "PLAYER-EQUIPPED" || !Number.isFinite(resolvedWeight)) {
            return;
        }
        outfitWeight = clamp(resolvedWeight, 0, 100);
        updateWeightReadout();
        window.meridianNifSyncWeight?.(outfitWeight.toString());
        document.getElementById("modelStatus").textContent =
            `REQUESTED PLAYER-EQUIPPED OUTFIT · WT ${Math.round(outfitWeight).toString().padStart(3, "0")}`;
    };

    sendCamera();
    sendLighting();
    updateOutfitReadout();
    updateWeightReadout();
    scheduleLayoutReport();
    requestAnimationFrame(animate);
})();
