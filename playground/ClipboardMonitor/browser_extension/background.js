// ClipboardMonitor Context Helper - Background Script (Service Worker)
// Handles communication between content scripts and native messaging host

const NATIVE_HOST_NAME = 'com.clipboardmonitor.context';

// Store the latest context from each tab
const tabContexts = new Map();

// Native messaging port (persistent connection)
let nativePort = null;

/**
 * Connect to the native messaging host
 */
function connectToNativeHost() {
    try {
        nativePort = chrome.runtime.connectNative(NATIVE_HOST_NAME);
        
        nativePort.onMessage.addListener((message) => {
            console.log('[ClipboardMonitor] Received from native:', message);
        });
        
        nativePort.onDisconnect.addListener(() => {
            console.log('[ClipboardMonitor] Native host disconnected');
            nativePort = null;
            
            // Reconnect after delay
            setTimeout(connectToNativeHost, 5000);
        });
        
        console.log('[ClipboardMonitor] Connected to native host');
    } catch (error) {
        console.error('[ClipboardMonitor] Failed to connect to native host:', error);
    }
}

/**
 * Send context to native host
 */
function sendToNativeHost(context) {
    if (!nativePort) {
        // Try one-shot message if persistent connection failed
        try {
            chrome.runtime.sendNativeMessage(NATIVE_HOST_NAME, context, (response) => {
                if (chrome.runtime.lastError) {
                    console.debug('[ClipboardMonitor] Native messaging error:', chrome.runtime.lastError);
                }
            });
        } catch (error) {
            console.debug('[ClipboardMonitor] Failed to send native message:', error);
        }
        return;
    }
    
    try {
        nativePort.postMessage(context);
    } catch (error) {
        console.error('[ClipboardMonitor] Error sending to native host:', error);
    }
}

/**
 * Handle messages from content scripts
 */
chrome.runtime.onMessage.addListener((message, sender, sendResponse) => {
    if (message.type === 'COPY_EVENT') {
        const tabId = sender.tab?.id;
        
        // Enrich context with tab info
        const enrichedContext = {
            ...message.data,
            tabId: tabId,
            tabUrl: sender.tab?.url,
            tabTitle: sender.tab?.title,
            windowId: sender.tab?.windowId,
            incognito: sender.tab?.incognito || false
        };
        
        // Store for later retrieval
        if (tabId) {
            tabContexts.set(tabId, enrichedContext);
        }
        
        // Send to native host
        sendToNativeHost(enrichedContext);
        
        // Also store in local storage for fallback
        chrome.storage.local.set({
            lastCopyContext: enrichedContext,
            lastCopyTime: Date.now()
        });
        
        sendResponse({ success: true });
    }
    
    return true;  // Keep message channel open
});

/**
 * Handle tab removal (cleanup)
 */
chrome.tabs.onRemoved.addListener((tabId) => {
    tabContexts.delete(tabId);
});

/**
 * Get context for a specific tab (for external queries)
 */
function getContextForTab(tabId) {
    return tabContexts.get(tabId) || null;
}

/**
 * Get the most recent context
 */
function getLatestContext() {
    let latest = null;
    let latestTime = 0;
    
    tabContexts.forEach((context) => {
        const time = new Date(context.timestamp).getTime();
        if (time > latestTime) {
            latestTime = time;
            latest = context;
        }
    });
    
    return latest;
}

// Initialize
console.log('[ClipboardMonitor] Background script initialized');

// Try to connect to native host (optional - extension works without it)
// connectToNativeHost();
