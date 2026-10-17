//
//  Console.swift
//
//  (i): Handles all Console related functions.
//
//  Created by Kjetil Kr Solberg on 24-09-2024.
//  Copyright © 2024 Kjetil Kr Solberg. All rights reserved.
//
//
// import
//
import Foundation
import WinSDK

///
/// Represents console color.
///
enum ConsoleColor : Int {
    case black = 30
    case red = 31
    case green = 32
    case yellow = 33
    case blue = 34
    case magenta = 35
    case cyan = 36
    case white = 37
    case reset = 0    
}
///
/// Console color modifier
///
enum ConsoleColorModifier : Int {
    case none = 0
    case bold = 1
}
///
/// Represents CMPlayer Console
///
internal class Console {
    //
    // static private constants.
    //     
    static private var savedInputMode: DWORD?   
    static private let concurrentQueue1 = DispatchQueue(label: "cqueue.cmplayer.windows.console.1", attributes: .concurrent)
    static private let concurrentQueue2 = DispatchQueue(label: "cqueue.cmplayer.windows.console.2", attributes: .concurrent)
    static private let concurrentQueue3 = DispatchQueue(label: "ConsoleResizeQueue", attributes: .concurrent)    
    static private let sigintSrcSIGINT = DispatchSource.makeSignalSource(signal: Int32(SIGINT), queue: Console.concurrentQueue1) // Ctrl+C
    //static private let sigintSrcSIGQUIT = DispatchSource.makeSignalSource(signal: Int32(SIGQUIT), queue: Console.concurrentQueue1)
    //static private let sigintSrcSIGILL = DispatchSource.makeSignalSource(signal: Int32(SIGILL), queue: Console.concurrentQueue1)
    //static private let sigintSrcSIGTRAP = DispatchSource.makeSignalSource(signal: Int32(SIGTRAP), queue: Console.concurrentQueue1)
    //static private let sigintSrcSIGABRT = DispatchSource.makeSignalSource(signal: Int32(SIGABRT), queue: Console.concurrentQueue1)
    //static private let sigintSrcSIGEMT = DispatchSource.makeSignalSource(signal: Int32(SIGEMT), queue: Console.concurrentQueue1)
    //static private let sigintSrcSIGFPE = DispatchSource.makeSignalSource(signal: Int32(SIGFPE), queue: Console.concurrentQueue1)
    //static private let sigintSrcSIGKILL = DispatchSource.makeSignalSource(signal: Int32(SIGKILL), queue: Console.concurrentQueue1)
    //static private let sigintSrcSIGBUS = DispatchSource.makeSignalSource(signal: Int32(SIGBUS), queue: Console.concurrentQueue1)
    //static private let sigintSrcSIGSEGV = DispatchSource.makeSignalSource(signal: Int32(SIGSEGV), queue: Console.concurrentQueue1)
    //static private let sigintSrcSIGSYS = DispatchSource.makeSignalSource(signal: Int32(SIGSYS), queue: Console.concurrentQueue1)
    //static private let sigintSrcSIGPIPE = DispatchSource.makeSignalSource(signal: Int32(SIGPIPE), queue: Console.concurrentQueue1)
    //static private let sigintSrcSIGALRM = DispatchSource.makeSignalSource(signal: Int32(SIGALRM), queue: Console.concurrentQueue1)
    //static private let sigintSrcSIGHUP = DispatchSource.makeSignalSource(signal: Int32(SIGHUP), queue: Console.concurrentQueue1)
    //static private let sigintSrcSIGTERM = DispatchSource.makeSignalSource(signal: Int32(SIGTERM), queue: Console.concurrentQueue1)
    //static private let sigintSrcSIGURG = DispatchSource.makeSignalSource(signal: Int32(SIGURG), queue: Console.concurrentQueue1)
    //static private let sigintSrcSIGSTOP = DispatchSource.makeSignalSource(signal: Int32(SIGSTOP), queue: Console.concurrentQueue1)
    //static private let sigintSrcSIGTSTP = DispatchSource.makeSignalSource(signal: Int32(SIGTSTP), queue: Console.concurrentQueue2)
    //static private let sigintSrcSIGCONT = DispatchSource.makeSignalSource(signal: Int32(SIGCONT), queue: Console.concurrentQueue1)
    //static private let sigintSrcSIGCHLD = DispatchSource.makeSignalSource(signal: Int32(SIGCHLD), queue: Console.concurrentQueue1)
    //static private let sigintSrcSIGTTIN = DispatchSource.makeSignalSource(signal: Int32(SIGTTIN), queue: Console.concurrentQueue1)
    //static private let sigintSrcSIGTTOU = DispatchSource.makeSignalSource(signal: Int32(SIGTTOU), queue: Console.concurrentQueue1)
    //static private let sigintSrcSIGIO = DispatchSource.makeSignalSource(signal: Int32(SIGIO), queue: Console.concurrentQueue1)
    //static private let sigintSrcSIGXCPU = DispatchSource.makeSignalSource(signal: Int32(SIGXCPU), queue: Console.concurrentQueue1)
    //static private let sigintSrcSIGXFSZ = DispatchSource.makeSignalSource(signal: Int32(SIGXFSZ), queue: Console.concurrentQueue1)
    //static private let sigintSrcSIGVTALRM = DispatchSource.makeSignalSource(signal: Int32(SIGVTALRM), queue: Console.concurrentQueue1)
    //static private let sigintSrcSIGPROF = DispatchSource.makeSignalSource(signal: Int32(SIGPROF), queue: Console.concurrentQueue1)
    //static private let sigintSrcSIGWINCH = DispatchSource.makeSignalSource(signal: Int32(SIGWINCH), queue: Console.concurrentQueue1) // Terminal Window Size Changed
    //static private let sigintSrcSIGUSR1 = DispatchSource.makeSignalSource(signal: Int32(SIGUSR1), queue: Console.concurrentQueue1)
    //static private let sigintSrcSIGUSR2 = DispatchSource.makeSignalSource(signal: Int32(SIGUSR2), queue: Console.concurrentQueue1)
    //static private let sigintSrcSIGTHR = DispatchSource.makeSignalSource(signal: Int32(SIGTHR), queue: Console.concurrentQueue1)
    //static private let sigintSrcSIGLIBRT = DispatchSource.makeSignalSource(signal: Int32(SIGLIBRT), queue: Console.concurrentQueue1)
    static private var resizeWorkItem: DispatchWorkItem?
    ///
    /// Clears console screen.
    ///
    static func clearScreen() -> Void {        
        // print space for entire window given standard app colors
        Console.printXY(1,1," ", g_rows*g_cols, .left, " ", ConsoleColor.black, ConsoleColorModifier.none, ConsoleColor.white, ConsoleColorModifier.none)
    }
    ///
    /// Clears console screen given colors.
    ///
    static func clearScreen(colorBg: ConsoleColor, modBg: ConsoleColorModifier, colorText: ConsoleColor, modText: ConsoleColorModifier) -> Void {
        // clear screen with given colors
        Console.printXY(1,1," ", g_rows*g_cols, .left, " ", colorBg, modBg, colorText, modText)
    }
    ///
    /// Clears console screen current theme.
    ///
    static func clearScreenCurrentTheme() -> Void {
        // switch color theme
        switch PlayerPreferences.colorTheme {
        // if default color theme
        case .Default:
            // clear screen given default color theme
            Console.printXY(1,1," ", g_rows*g_cols, .left, " ", ConsoleColor.black, ConsoleColorModifier.none, ConsoleColor.white, ConsoleColorModifier.none)
        // if blue color theme
        case .Blue:
            // clear screen given blue color theme
            Console.printXY(1,1," ", g_rows*g_cols, .left, " ", ConsoleColor.blue, ConsoleColorModifier.none, ConsoleColor.white, ConsoleColorModifier.none)
        // if black color theme
        case .Black:
            // clear screen given black color theme
            Console.printXY(1,1," ", g_rows*g_cols, .left, " ", ConsoleColor.black, ConsoleColorModifier.none, ConsoleColor.white, ConsoleColorModifier.none)
        case .Custom:
            // clear screen given default color theme
            Console.printXY(1,1," ", g_rows*g_cols, .left, " ", ConsoleColor.black, ConsoleColorModifier.none, ConsoleColor.white, ConsoleColorModifier.none)
        }        
    }    
    ///
    /// Hides console cursor.
    ///
    static func hideCursor() -> Void {
        // hide cursor in terminal
        print("\u{001B}[?25l")
    }
    ///
    /// Shows console cursor.
    ///
    static func showCursor() -> Void {
        // show cursor in terminal
        print("\u{001B}[?25h")
    }
    ///
    /// Turns console echo off.
    ///
    @discardableResult
    static func echoOff() -> Bool {
        let input = GetStdHandle(STD_INPUT_HANDLE)
        var mode: DWORD = 0

        guard GetConsoleMode(input, &mode) else {
            return false
        }

        let newMode = (mode & ~DWORD(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT)) | DWORD(ENABLE_VIRTUAL_TERMINAL_INPUT)

        guard SetConsoleMode(input, newMode) else {
            return false
        }

        if savedInputMode == nil {
            savedInputMode = mode
        }
        return true
    }
    ///
    /// Turns console echo on.
    ///
    @discardableResult
    static func echoOn() -> Bool {
        guard let mode = savedInputMode else {
            return true
        }

        let input = GetStdHandle(STD_INPUT_HANDLE)
        guard SetConsoleMode(input, mode) else {
            return false
        }

        savedInputMode = nil
        return true
    }
    ///
    /// Applies color to text string.
    ///
    /// parameter colorBg: Background console color.
    /// parameter modifierBg: Background console color modifier.
    /// parameter colorText: Text console color.
    /// parameter modifierText: Text console color modifier.
    /// parameter text: Text to output to console.
    ///
    /// returns: String to be written to console using print.
    ///
    static func applyTextColor(colorBg: ConsoleColor, modifierBg:  ConsoleColorModifier, colorText: ConsoleColor, modifierText: ConsoleColorModifier, text: String) -> String {
        // create constant for adding bold or not modifier to colorText
        let addToText: String = (modifierText == ConsoleColorModifier.bold) ? ";1": ""
        // create constant for adding bold or not modifier to colorBg
        let addToBg: String = (modifierBg == ConsoleColorModifier.bold) ? ";1": ""
        // return string to be printed out somewhere on screen
        return "\u{001B}[\(colorText.rawValue)\(addToText)m\u{001B}[\(colorBg.rawValue+10)\(addToBg)m\(text)\u{001B}[0m"
    }
    ///
    /// Sets terminal size in character length.
    ///
    /// parameter width. Number of characters in x axis.
    /// parameter height: Number of characters in y axis.
    ///
    static func setTerminalSize(width: Int, height: Int) -> Void {
        // set terminal size width x height
        print("\u{001B}[8;\(height);\(width)t", terminator: "")
    }
    ///
    /// Moves console position.
    ///
    /// parameter x: Console x position.
    /// parameter y: Console y position.
    ///
    static func gotoXY(_ x: Int, _ y: Int) -> Void
    {
        // goto x, y coordinate on screen
        print("\u{001B}[\(y);\(x)H", terminator: "")
    }
    ///
    /// Prints a string to console at given position.
    ///
    /// parameter x: Console x position.
    /// parameter y: Console y position.
    /// parameter text: Text to be written to console.
    /// parameter maxLength: Maximum length of string to be written.
    /// parameter padding: How should string content be aligned.
    /// parameter paddingChar: What char should be applied with padding to maximum length.
    /// parameter bgColor: Console background color.
    /// parameter modifierBg: Console background color modifier.
    /// parameter colorText: Console text color.
    /// parameter modifierText: Console text color modifier
    ///
    static func printXY(_ x: Int,_ y: Int,_ text: String,_ maxLength: Int,_ padding: PrintPaddingTextAlign,_ paddingChar: Character, _ bgColor: ConsoleColor, _ modifierBg: ConsoleColorModifier, _ colorText: ConsoleColor,_ modifierText: ConsoleColorModifier) -> Void {
        // create new message text based on given length and padding
        let nmsg = text.convertStringToLengthPaddedString(maxLength, padding, paddingChar)
        // print it to screen
        print("\u{001B}[\(y);\(x)H\(Console.applyTextColor(colorBg: bgColor, modifierBg: modifierBg, colorText: colorText, modifierText: modifierText, text: nmsg))", terminator: "")
    }    
    ///
    /// Resets console colors
    /// 
    static func resetConsoleColors()
    {
        // reset console colors
        print("\u{001B}[0m")
    }
    ///
    ///
    /// 
    static func terminalSize() -> (rows: Int, columns: Int)? {
        let output = GetStdHandle(STD_OUTPUT_HANDLE)
        var info = CONSOLE_SCREEN_BUFFER_INFO()

        guard GetConsoleScreenBufferInfo(output, &info) else {
            return nil
        }

        return (
            rows: Int(info.srWindow.Bottom) - Int(info.srWindow.Top) + 1,
            columns: Int(info.srWindow.Right) - Int(info.srWindow.Left) + 1
        )
    }
    ///
    /// initializes console.
    ///
    static func initialize() -> Void {        
        // get terminal size
        let w = terminalSize()         
        // return if nil
        guard w != nil else {
            print("Console.initialize failed.")
            print(" > GetConsoleScreenBufferInfo failed.");
            print("   > \(windowsErrorString())")
            exit(ExitCodes.ERROR_CONSOLE_INIT.rawValue);
        }       
        // set global g_rows to terminal size rows
        g_rows = Int(w!.rows)
        // set global g_cols to terminal size cols
        g_cols = Int(w!.columns)                        
        // hide cursor
        Console.hideCursor()
        // echo off
        Console.echoOff()
        // ignore SIGTSTP (Ctrl+Z)
        //signal(SIGTSTP,SIG_IGN)
        // ignore SIGINT (Ctrl+C) (we will handle it)
        signal(SIGINT,SIG_IGN)
        // at exit run this code
        atexit( {
            // show cursor
            Console.showCursor()
            // echo on
            Console.echoOn()
        })
        // Respond to Ctrl+C
        sigintSrcSIGINT.setEventHandler {
            // windows that repaint async does not paint over the printed message before exit
            g_doNotPaint = true 
            // create message
            let msg: String = "CMPlayer exited due to Ctrl+C."            
            // clear screen
            Console.clearScreen()
            // goto 1, 1
            Console.gotoXY(1, 1)
            // reset console colors
            Console.resetConsoleColors()
            // clear screen
            system("clear")
            // print message
            print(msg)
            // exit application
            exit(ExitCodes.ERROR_CANCEL.rawValue)
        }
        // start processing signal
        sigintSrcSIGINT.resume()
        // Respond to window resize
        _ = Console.enableTerminalResizeEvents()
        concurrentQueue3.async {
            Console.processConsoleEvents()
        }        
    }// initialize
    /// 
    /// Enables terminal resize events
    /// 
    static func enableTerminalResizeEvents() -> Bool {
        let hInput = GetStdHandle(STD_INPUT_HANDLE)

        var mode: DWORD = 0

        guard GetConsoleMode(hInput, &mode) else {
            return false
        }
        
        return SetConsoleMode(hInput,mode | DWORD(ENABLE_WINDOW_INPUT))
    }
    ///
    /// Processes console events
    ///
    static func processConsoleEvents() {
        
        let hOutput = GetStdHandle(STD_OUTPUT_HANDLE)

        var previousRows = g_rows
        var previousCols = g_cols

        while !Thread.current.isCancelled {
            
            var info = CONSOLE_SCREEN_BUFFER_INFO()
            
            if GetConsoleScreenBufferInfo(hOutput, &info) {
                  
                let rows = Int(
                    info.srWindow.Bottom - info.srWindow.Top + 1
                )

                let cols = Int(
                    info.srWindow.Right - info.srWindow.Left + 1
                )

                if rows != previousRows || cols != previousCols {
                    g_termSizeIsChanging = true
                    Console.clearScreenCurrentTheme()

                    // Cancel the previously scheduled resize notification
                    Console.resizeWorkItem?.cancel()
        
                    previousRows = rows
                    previousCols = cols                                    
                    
                    g_rows = rows
                    g_cols = cols

                    // Create a new delayed notification
                    let workItem = DispatchWorkItem {
                        if !g_tscpStack.isEmpty {
                            g_tscpStack.last?.terminalSizeHasChanged()
                            g_termSizeIsChanging = false
                        }
                    }

                    // Remember the new work item
                    Console.resizeWorkItem = workItem
                                    
                    // Execute only after 250 ms without another SIGWINCH
                    concurrentQueue2.asyncAfter(
                        deadline: .now() + .milliseconds(130),
                        execute: workItem
                    )                    
                }
            }

            Thread.sleep(forTimeInterval: 0.1)
        }
    }
}// Console
