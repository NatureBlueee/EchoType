// ClipboardMonitor Context Helper - Content Script
// Runs on every page to capture copy events and context

(function() {
    'use strict';

    // Configuration
    const CONFIG = {
        contextParagraphs: 2,  // Number of paragraphs before/after selection
        maxContentLength: 10000,  // Max characters to capture
        debounceMs: 100  // Debounce time for copy events
    };

    let lastCopyTime = 0;

    /**
     * Get the selection context (text before and after the selection)
     */
    function getSelectionContext() {
        const selection = window.getSelection();
        if (!selection || selection.rangeCount === 0) {
            return null;
        }

        const range = selection.getRangeAt(0);
        const selectedText = selection.toString();

        if (!selectedText.trim()) {
            return null;
        }

        // Get the container element
        let container = range.commonAncestorContainer;
        if (container.nodeType === Node.TEXT_NODE) {
            container = container.parentElement;
        }

        // Try to find a meaningful context container (paragraph, article, section)
        let contextContainer = container;
        const meaningfulTags = ['P', 'ARTICLE', 'SECTION', 'DIV', 'LI', 'TD', 'BLOCKQUOTE'];
        while (contextContainer && contextContainer !== document.body) {
            if (meaningfulTags.includes(contextContainer.tagName)) {
                break;
            }
            contextContainer = contextContainer.parentElement;
        }

        // Get surrounding text
        let beforeText = '';
        let afterText = '';

        if (contextContainer && contextContainer !== document.body) {
            const fullText = contextContainer.innerText || contextContainer.textContent;
            const selectionStart = fullText.indexOf(selectedText);
            
            if (selectionStart !== -1) {
                beforeText = fullText.substring(0, selectionStart);
                afterText = fullText.substring(selectionStart + selectedText.length);
                
                // Limit length
                beforeText = beforeText.slice(-500);
                afterText = afterText.slice(0, 500);
            }
        }

        return {
            selectedText: selectedText,
            beforeText: beforeText.trim(),
            afterText: afterText.trim(),
            containerTag: contextContainer ? contextContainer.tagName : null
        };
    }

    /**
     * Get page metadata
     */
    function getPageMetadata() {
        const metadata = {
            url: window.location.href,
            title: document.title,
            domain: window.location.hostname
        };

        // Open Graph metadata
        const ogTitle = document.querySelector('meta[property="og:title"]');
        const ogDescription = document.querySelector('meta[property="og:description"]');
        const ogImage = document.querySelector('meta[property="og:image"]');

        if (ogTitle) metadata.ogTitle = ogTitle.content;
        if (ogDescription) metadata.ogDescription = ogDescription.content;
        if (ogImage) metadata.ogImage = ogImage.content;

        // Standard metadata
        const description = document.querySelector('meta[name="description"]');
        const keywords = document.querySelector('meta[name="keywords"]');
        const author = document.querySelector('meta[name="author"]');

        if (description) metadata.description = description.content;
        if (keywords) metadata.keywords = keywords.content;
        if (author) metadata.author = author.content;

        // Canonical URL
        const canonical = document.querySelector('link[rel="canonical"]');
        if (canonical) metadata.canonicalUrl = canonical.href;

        return metadata;
    }

    /**
     * Get visible text around the selection
     */
    function getVisibleContext() {
        const scrollTop = window.scrollY;
        const viewportHeight = window.innerHeight;
        
        // Get all text elements in the viewport
        const elements = document.querySelectorAll('p, h1, h2, h3, h4, h5, h6, li, td, th, span, div');
        const visibleTexts = [];

        elements.forEach(el => {
            const rect = el.getBoundingClientRect();
            // Check if element is in viewport
            if (rect.top >= 0 && rect.top <= viewportHeight && el.innerText.trim()) {
                visibleTexts.push(el.innerText.trim());
            }
        });

        return visibleTexts.slice(0, 10).join('\n').slice(0, 2000);
    }

    /**
     * Handle copy event
     */
    function handleCopy(event) {
        const now = Date.now();
        if (now - lastCopyTime < CONFIG.debounceMs) {
            return;  // Debounce
        }
        lastCopyTime = now;

        try {
            const context = {
                timestamp: new Date().toISOString(),
                page: getPageMetadata(),
                selection: getSelectionContext(),
                visibleContext: getVisibleContext()
            };

            // Send to background script
            chrome.runtime.sendMessage({
                type: 'COPY_EVENT',
                data: context
            }, response => {
                if (chrome.runtime.lastError) {
                    console.debug('[ClipboardMonitor] Error sending message:', chrome.runtime.lastError);
                }
            });

        } catch (error) {
            console.debug('[ClipboardMonitor] Error capturing context:', error);
        }
    }

    // Listen for copy events
    document.addEventListener('copy', handleCopy, true);

    // Also listen for cut events (they also put content in clipboard)
    document.addEventListener('cut', handleCopy, true);

    console.debug('[ClipboardMonitor] Content script loaded');
})();
