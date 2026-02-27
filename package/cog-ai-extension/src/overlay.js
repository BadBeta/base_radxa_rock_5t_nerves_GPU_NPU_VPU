/*
 * Cog AI Overlay — injected into web pages by cog-ai-extension
 *
 * Creates floating control buttons for real-time YOLO object detection
 * and Whisper transcription overlays.
 *
 * Communication uses the native WebSocket bridge provided by the C extension:
 *   window.__cogAiSend(jsonString)  — send to backend
 *   window.__cogAiConnected         — boolean, set by native code
 *   window.__cogAiOnMessage         — called by native code with incoming JSON
 *   window.__cogAiOnConnect         — called by native code on connect
 *   window.__cogAiOnDisconnect      — called by native code on disconnect
 */
(function() {
  'use strict';

  // Prevent double-injection
  if (window.__cogAiOverlay) return;
  window.__cogAiOverlay = true;

  // ========================================================================
  // State
  // ========================================================================

  var yoloEnabled = false;
  var whisperEnabled = false;

  // ========================================================================
  // Native WebSocket Bridge
  // ========================================================================

  function isConnected() {
    return !!window.__cogAiConnected;
  }

  function sendJSON(obj) {
    if (isConnected() && window.__cogAiSend) {
      window.__cogAiSend(JSON.stringify(obj));
    }
  }

  // Register message handler — called by native code
  window.__cogAiOnMessage = function(jsonStr) {
    try {
      var msg = JSON.parse(jsonStr);
      if (msg.type === 'yolo_result') {
        renderDetections(msg.detections);
      } else if (msg.type === 'yolo_error') {
        console.error('[CogAI] YOLO error:', msg.error);
        renderSubtitle('YOLO: ' + msg.error);
      } else if (msg.type === 'whisper_result') {
        renderWhisper(msg.text);
      } else if (msg.type === 'whisper_status') {
        renderWhisper(msg.text);
      }
    } catch(e) {
      console.error('[CogAI] Parse error:', e);
    }
  };

  // Called by native code when WebSocket connects
  window.__cogAiOnConnect = function() {
    console.log('[CogAI] Native WebSocket connected');
    updateButtonStates();
    renderSubtitle('Connected to AI backend');
  };

  // Called by native code when WebSocket disconnects
  window.__cogAiOnDisconnect = function() {
    console.log('[CogAI] Native WebSocket disconnected');
    updateButtonStates();
  };

  // ========================================================================
  // Video Element Finder (for SVG overlay positioning)
  // ========================================================================

  function findVideoElement() {
    var v = document.querySelector('video');
    if (v && v.readyState >= 2 && v.videoWidth > 0) return v;
    return null;
  }

  // ========================================================================
  // Detection Rendering (SVG Overlay)
  // ========================================================================

  var svgOverlay = null;

  function ensureSVGOverlay() {
    var video = findVideoElement();
    if (!video) return null;

    // Position SVG fixed over the video's actual screen rect
    var rect = video.getBoundingClientRect();

    if (!svgOverlay) {
      svgOverlay = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
      svgOverlay.id = 'cog-ai-detections';
      svgOverlay.setAttribute('xmlns', 'http://www.w3.org/2000/svg');
      document.body.appendChild(svgOverlay);
    }

    svgOverlay.style.cssText = [
      'position: fixed',
      'top: ' + rect.top + 'px',
      'left: ' + rect.left + 'px',
      'width: ' + rect.width + 'px',
      'height: ' + rect.height + 'px',
      'pointer-events: none',
      'z-index: 9998'
    ].join(';');

    console.log('[CogAI] SVG overlay positioned at', rect.left, rect.top, rect.width, 'x', rect.height);

    return svgOverlay;
  }

  function renderDetections(detections) {
    var svg = ensureSVGOverlay();
    if (!svg) return;

    while (svg.firstChild) svg.removeChild(svg.firstChild);

    if (!detections || detections.length === 0) return;

    var svgNS = 'http://www.w3.org/2000/svg';

    detections.forEach(function(det) {
      var x1 = (det.x1 * 100) + '%';
      var y1 = (det.y1 * 100) + '%';
      var w  = ((det.x2 - det.x1) * 100) + '%';
      var h  = ((det.y2 - det.y1) * 100) + '%';
      var color = det.color || '#00FF00';

      var rect = document.createElementNS(svgNS, 'rect');
      rect.setAttribute('x', x1);
      rect.setAttribute('y', y1);
      rect.setAttribute('width', w);
      rect.setAttribute('height', h);
      rect.setAttribute('fill', 'none');
      rect.setAttribute('stroke', color);
      rect.setAttribute('stroke-width', '2');
      svg.appendChild(rect);

      var labelBg = document.createElementNS(svgNS, 'rect');
      labelBg.setAttribute('x', x1);
      labelBg.setAttribute('y', y1);
      labelBg.setAttribute('width', '120');
      labelBg.setAttribute('height', '20');
      labelBg.setAttribute('fill', color);
      labelBg.setAttribute('opacity', '0.7');
      svg.appendChild(labelBg);

      var text = document.createElementNS(svgNS, 'text');
      text.setAttribute('x', (det.x1 * 100 + 0.3) + '%');
      text.setAttribute('y', (det.y1 * 100 + 1.5) + '%');
      text.setAttribute('fill', 'white');
      text.setAttribute('font-size', '12');
      text.setAttribute('font-family', 'monospace');
      text.textContent = det.label + ' ' + (det.confidence * 100).toFixed(0) + '%';
      svg.appendChild(text);
    });
  }

  // ========================================================================
  // Subtitle / Status Rendering
  // ========================================================================

  var statusBar = null;
  var statusTimeout = null;
  var tickerBar = null;
  var whisperTexts = [];
  var MAX_WHISPER_TEXTS = 5;

  function makeBar(id, bottom, bg) {
    var bar = document.createElement('div');
    bar.id = id;
    bar.style.cssText = [
      'position: fixed',
      'bottom: ' + bottom, 'left: 10%', 'right: 10%',
      'background: ' + bg,
      'color: white',
      'font-size: 18px',
      'padding: 10px 20px',
      'text-align: center',
      'border-radius: 8px',
      'z-index: 10000',
      'pointer-events: none',
      'display: none',
      'font-family: sans-serif'
    ].join(';');
    document.body.appendChild(bar);
    return bar;
  }

  function renderSubtitle(text) {
    if (!text || text.trim() === '') return;
    if (!statusBar) statusBar = makeBar('cog-ai-status', '80px', 'rgba(0,0,0,0.8)');
    statusBar.textContent = text;
    statusBar.style.display = 'block';
    if (statusTimeout) clearTimeout(statusTimeout);
    statusTimeout = setTimeout(function() {
      statusBar.style.display = 'none';
    }, 8000);
  }

  function ensureTickerBar() {
    if (tickerBar) return tickerBar;

    // Inject CSS keyframes once
    var style = document.createElement('style');
    style.textContent = '@keyframes cogai-ticker { 0% { transform: translateX(0); } 100% { transform: translateX(-50%); } }';
    document.head.appendChild(style);

    tickerBar = document.createElement('div');
    tickerBar.id = 'cog-ai-ticker';
    tickerBar.style.cssText = [
      'position: fixed',
      'bottom: 30px', 'left: 5%', 'right: 5%',
      'background: rgba(30,64,175,0.9)',
      'color: #00FF00',
      'font-size: 16px',
      'padding: 8px 0',
      'border-radius: 8px',
      'z-index: 10000',
      'pointer-events: none',
      'font-family: monospace',
      'white-space: nowrap',
      'overflow: hidden',
      'display: none'
    ].join(';');

    var inner = document.createElement('span');
    inner.id = 'cog-ai-ticker-inner';
    inner.style.cssText = [
      'display: inline-block',
      'padding-left: 100%',
      'animation: cogai-ticker 30s linear infinite'
    ].join(';');

    tickerBar.appendChild(inner);
    document.body.appendChild(tickerBar);
    return tickerBar;
  }

  function updateTickerText() {
    var bar = ensureTickerBar();
    var inner = document.getElementById('cog-ai-ticker-inner');
    if (!inner) return;

    if (whisperTexts.length === 0) {
      bar.style.display = 'none';
      return;
    }

    var fullText = whisperTexts.join('  \u2503  ');
    // Duplicate for seamless loop
    inner.textContent = fullText + '  \u2503  ' + fullText + '  \u2503  ';
    // Adjust speed based on text length (longer text = slower)
    var duration = Math.max(15, fullText.length * 0.2);
    inner.style.animation = 'cogai-ticker ' + duration + 's linear infinite';
    bar.style.display = 'block';
  }

  function renderWhisper(text) {
    if (!text || text.trim() === '') return;
    // Status messages (non-transcription) show as subtitle
    if (text.startsWith('Whisper:')) {
      renderSubtitle(text);
      return;
    }
    // Add transcription to rolling buffer
    whisperTexts.push(text);
    if (whisperTexts.length > MAX_WHISPER_TEXTS) {
      whisperTexts.shift();
    }
    updateTickerText();
  }

  // ========================================================================
  // UI Buttons
  // ========================================================================

  function createButtons() {
    var container = document.createElement('div');
    container.id = 'cog-ai-buttons';
    container.style.cssText = [
      'position: fixed',
      'top: 10px', 'right: 10px',
      'z-index: 10001',
      'display: flex',
      'flex-direction: column',
      'gap: 8px'
    ].join(';');

    // BACK button — navigate back to launcher
    var backBtn = makeButton('\u2190 BACK', '#dc2626', function() {
      if (yoloEnabled) {
        yoloEnabled = false;
        sendJSON({ type: 'yolo_stop' });
        if (svgOverlay) {
          while (svgOverlay.firstChild) svgOverlay.removeChild(svgOverlay.firstChild);
        }
      }
      if (whisperEnabled) {
        whisperEnabled = false;
        sendJSON({ type: 'whisper_stop' });
      }
      sendJSON({ type: 'navigate_back' });
      // Fallback: navigate directly if not connected
      if (!isConnected()) {
        window.location.href = 'http://localhost:80';
      }
    });
    backBtn.id = 'cog-ai-back-btn';
    container.appendChild(backBtn);

    // YOLO button — server-side DRM screen capture + NPU inference
    var yoloRepositionInterval = null;
    var yoloBtn = makeButton('YOLO', '#333', function() {
      if (!isConnected()) {
        renderSubtitle('Not connected to AI backend');
        return;
      }
      yoloEnabled = !yoloEnabled;
      if (yoloEnabled) {
        yoloBtn.style.background = '#22c55e';
        yoloBtn.textContent = 'YOLO ON';
        sendJSON({ type: 'yolo_start' });
        // Re-position SVG overlay to track video layout changes
        yoloRepositionInterval = setInterval(function() {
          if (yoloEnabled) ensureSVGOverlay();
        }, 3000);
        renderSubtitle('YOLO: detecting objects (DRM capture)...');
      } else {
        yoloBtn.style.background = '#333';
        yoloBtn.textContent = 'YOLO';
        sendJSON({ type: 'yolo_stop' });
        if (yoloRepositionInterval) { clearInterval(yoloRepositionInterval); yoloRepositionInterval = null; }
        if (svgOverlay) {
          while (svgOverlay.firstChild) svgOverlay.removeChild(svgOverlay.firstChild);
        }
      }
    });
    yoloBtn.id = 'cog-ai-yolo-btn';
    container.appendChild(yoloBtn);

    // WHISPER button
    var whisperBtn = makeButton('WHISPER', '#333', function() {
      if (!isConnected()) {
        renderSubtitle('Not connected to AI backend');
        return;
      }
      whisperEnabled = !whisperEnabled;
      if (whisperEnabled) {
        whisperBtn.style.background = '#3b82f6';
        whisperBtn.textContent = 'WHISPER ON';
        sendJSON({ type: 'whisper_start' });
        renderSubtitle('Whisper listening...');
      } else {
        whisperBtn.style.background = '#333';
        whisperBtn.textContent = 'WHISPER';
        sendJSON({ type: 'whisper_stop' });
        // Clear ticker after a delay so last transcription is visible
        setTimeout(function() {
          if (!whisperEnabled) {
            whisperTexts = [];
            updateTickerText();
          }
        }, 10000);
      }
    });
    whisperBtn.id = 'cog-ai-whisper-btn';
    container.appendChild(whisperBtn);

    document.body.appendChild(container);
  }

  function makeButton(label, bg, onclick) {
    var btn = document.createElement('button');
    btn.textContent = label;
    btn.style.cssText = [
      'background: ' + bg,
      'color: white',
      'border: 1px solid rgba(255,255,255,0.3)',
      'border-radius: 8px',
      'padding: 8px 16px',
      'font-size: 14px',
      'font-weight: bold',
      'font-family: monospace',
      'cursor: pointer',
      'min-width: 90px',
      'text-align: center',
      'touch-action: manipulation',
      '-webkit-tap-highlight-color: transparent'
    ].join(';');
    btn.addEventListener('click', onclick);
    return btn;
  }

  function updateButtonStates() {
    var connected = isConnected();
    var yoloBtn = document.getElementById('cog-ai-yolo-btn');
    var whisperBtn = document.getElementById('cog-ai-whisper-btn');
    if (!connected) {
      if (yoloBtn) yoloBtn.style.opacity = '0.5';
      if (whisperBtn) whisperBtn.style.opacity = '0.5';
    } else {
      if (yoloBtn) yoloBtn.style.opacity = '1';
      if (whisperBtn) whisperBtn.style.opacity = '1';
    }
  }

  // ========================================================================
  // Initialization
  // ========================================================================

  function init() {
    console.log('[CogAI] Overlay initializing (native bridge mode)');
    console.log('[CogAI] __cogAiConnected =', window.__cogAiConnected);
    console.log('[CogAI] __cogAiSend =', typeof window.__cogAiSend);
    createButtons();
    updateButtonStates();

    // Show initial connection status
    if (isConnected()) {
      renderSubtitle('AI backend connected \u2014 press YOLO or WHISPER');
    } else {
      renderSubtitle('Connecting to AI backend...');
    }
  }

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', init);
  } else {
    init();
  }

})();
