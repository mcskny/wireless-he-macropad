import Cocoa
import WebKit
import Network

final class AppDelegate: NSObject, NSApplicationDelegate, WKNavigationDelegate, WKDownloadDelegate, NSWindowDelegate {
    var window: NSWindow!
    var web: WKWebView!
    var process: Process?
    let port = 18768
    var retries = 0

    func applicationDidFinishLaunching(_ notification: Notification) {
        let cfg = WKWebViewConfiguration()
        web = WKWebView(frame: .zero, configuration: cfg)
        web.navigationDelegate = self
        window = NSWindow(contentRect: NSRect(x: 0, y: 0, width: 1280, height: 850), styleMask: [.titled, .closable, .miniaturizable, .resizable], backing: .buffered, defer: false)
        window.minSize = NSSize(width: 740, height: 600)
        window.title = "Wireless HE Studio"
        window.contentView = web
        window.center()
        window.delegate = self
        window.makeKeyAndOrderFront(nil)
        NSApp.activate(ignoringOtherApps: true)
        guard let project = Bundle.main.object(forInfoDictionaryKey: "TestProjectPath") as? String,
              let python = Bundle.main.object(forInfoDictionaryKey: "TestPythonPath") as? String else {
            showError("Uygulamanın test projesi yolu bulunamadı."); return
        }
        let proc = Process()
        proc.executableURL = URL(fileURLWithPath: python)
        proc.arguments = ["-u", "server.py", "--http-port", String(port)]
        proc.currentDirectoryURL = URL(fileURLWithPath: project)
        proc.standardOutput = FileHandle.nullDevice
        proc.standardError = FileHandle.nullDevice
        do {
            try proc.run()
            process = proc
            DispatchQueue.main.asyncAfter(deadline: .now() + 0.8) { [weak self] in
                guard let self = self else { return }
                if !proc.isRunning { self.showError("Yerel sunucu başlatılamadı. 18768 portu kullanımda olabilir veya Python ortamı bulunamıyor."); return }
                self.loadStudio()
            }
        } catch { showError("Yerel sunucu başlatılamadı: \(error.localizedDescription)") }
    }
    func loadStudio() { web.load(URLRequest(url: URL(string: "http://127.0.0.1:\(port)/demo")!)) }
    func webView(_ webView: WKWebView, didFailProvisionalNavigation navigation: WKNavigation!, withError error: Error) {
        if retries < 5, process?.isRunning == true {
            retries += 1
            DispatchQueue.main.asyncAfter(deadline: .now() + 0.5) { [weak self] in self?.loadStudio() }
        } else { showError("Yerel arayüze bağlanılamadı. Uygulamayı yeniden açmayı deneyin.") }
    }
    func showError(_ text: String) {
        let alert = NSAlert(); alert.messageText = "Wireless HE Studio açılamadı"; alert.informativeText = text; alert.runModal()
        NSApp.terminate(nil)
    }
    func webView(_ webView: WKWebView, decidePolicyFor navigationAction: WKNavigationAction, decisionHandler: @escaping (WKNavigationActionPolicy) -> Void) {
        decisionHandler(navigationAction.shouldPerformDownload ? .download : .allow)
    }
    func webView(_ webView: WKWebView, navigationAction: WKNavigationAction, didBecome download: WKDownload) {
        download.delegate = self
    }
    func download(_ download: WKDownload, decideDestinationUsing response: URLResponse, suggestedFilename: String, completionHandler: @escaping (URL?) -> Void) {
        let panel = NSSavePanel()
        panel.nameFieldStringValue = suggestedFilename
        panel.begin { result in completionHandler(result == .OK ? panel.url : nil) }
    }
    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool { true }
    func applicationWillTerminate(_ notification: Notification) { if let p = process, p.isRunning { p.interrupt(); p.waitUntilExit() } }
}
let app = NSApplication.shared
let delegate = AppDelegate()
app.delegate = delegate
app.setActivationPolicy(.regular)
let menuBar = NSMenu()
let appMenuItem = NSMenuItem()
menuBar.addItem(appMenuItem)
let appMenu = NSMenu()
appMenu.addItem(withTitle: "Wireless HE Studio’dan Çık", action: #selector(NSApplication.terminate(_:)), keyEquivalent: "q")
appMenuItem.submenu = appMenu
app.mainMenu = menuBar
app.run()
