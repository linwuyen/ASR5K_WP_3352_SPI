Private Type dCell
    str As String
    ptr As Range
End Type

Function GenerateKeywords(inputString As String) As String
    Dim words() As String
    Dim keyword As String
    Dim i As Integer
    
    ' Split the input string into individual words
    words = Split(inputString, " ")
    
    ' Loop through each word and generate the corresponding keyword
    For i = LBound(words) To UBound(words)
        keyword = words(i)
        
        ' Determine the length of the keyword based on the rules
        If Len(keyword) < 3 Then
            ' For keywords with length less than 3, use the entire word
            keyword = UCase(Left(keyword, 1)) & LCase(Mid(keyword, 2))
        ElseIf Left(keyword, 1) Like "[AEIOUaeiou]" Then
            ' For keywords starting with a vowel, use the left 4 characters
            keyword = UCase(Left(keyword, 1)) & LCase(Mid(keyword, 2, 3))
        Else
            ' For keywords starting with a consonant, use the left 3 characters
            keyword = UCase(Left(keyword, 1)) & LCase(Mid(keyword, 2, 2))
        End If
        
        ' Append the keyword to the result string
        GenerateKeywords = GenerateKeywords & keyword
    Next i
End Function


Function GenerateLongFormKeywords(inputString As String) As String
    Dim words() As String
    Dim keyword As String
    Dim longFormKeyword As String
    Dim i As Integer
    
    ' Split the input string into individual words
    words = Split(inputString, " ")
    
    ' Loop through each word and generate the corresponding long form of keyword
    For i = LBound(words) To UBound(words)
        keyword = words(i)
        
        ' Remove symbols from the keyword
        keyword = RemoveSymbols(keyword)
        
        ' Determine the length of the keyword based on the rules
        If Len(keyword) < 3 Then
            ' For keywords with length less than 3, use the entire word
            longFormKeyword = LCase(keyword)
        ElseIf Left(keyword, 1) Like "[AEIOUaeiou]" Then
            ' For keywords starting with a vowel, use the left 4 characters
            longFormKeyword = UCase(Left(keyword, 4)) & LCase(Mid(keyword, 5))
        Else
            ' For keywords starting with a consonant, use the left 3 characters
            longFormKeyword = UCase(Left(keyword, 3)) & LCase(Mid(keyword, 4))
        End If
        
        ' Append the long form of keyword to the result string
        GenerateLongFormKeywords = GenerateLongFormKeywords & longFormKeyword
    Next i
End Function

Function RemoveSymbols(inputString As String) As String
    Dim symbols As String
    Dim i As Integer
    
    ' Define symbols to be removed
    symbols = "!@#$%^&*()-=_+[]{}|;:,.<>/?`~'"
    
    ' Loop through each symbol and remove it from the input string
    For i = 1 To Len(symbols)
        inputString = Replace(inputString, Mid(symbols, i, 1), "")
    Next i
    
    ' Return the string without symbols
    RemoveSymbols = inputString
End Function

Function FillSpaceToLength(inputString As String, desiredLength As Integer) As String
    Dim currentLength As Integer
    currentLength = Len(inputString)
    
    If currentLength >= desiredLength Then
        ' If the current length is equal to or greater than the desired length, return the input string as is
        FillSpaceToLength = inputString
    Else
        ' Calculate the number of spaces needed to reach the desired length
        Dim spacesNeeded As Integer
        spacesNeeded = desiredLength - currentLength
        
        ' Determine the number of times the spaces should be repeated
        Dim repetition As Integer
        repetition = WorksheetFunction.Ceiling(spacesNeeded / Len(" "), 1)
        
        ' Create a string of spaces to fill the remaining length
        Dim spaces As String
        spaces = WorksheetFunction.Rept(" ", repetition)
        
        ' Append the spaces to the input string
        FillSpaceToLength = inputString & spaces
    End If
End Function

Function GetComputerName() As String
    GetComputerName = Environ("COMPUTERNAME")
End Function


Function getNameString(ByRef src As String)

getNameString = GenerateLongFormKeywords(src)

End Function

Function getShortName(ByRef src As String)

getShortName = GenerateKeywords(src)

End Function

Function fillSpace(src As String, ssize As Integer)

fillSpace = FillSpaceToLength(src, ssize)

End Function


Function getRegName(DataFormat As String, MenuID As String)

    Select Case DataFormat
    
        Case "T_U16"
            getRegName = "u16" & getNameString(MenuID)
        Case "T_S16", "T_Q15"
            getRegName = "s16" & getNameString(MenuID)
        Case "T_U32"
            getRegName = "u32" & getNameString(MenuID)
        Case "T_S32"
           getRegName = "s32" & getNameString(MenuID)
        Case "T_F32"
            getRegName = "f32" & getNameString(MenuID)
        Case "T_D64"
            getRegName = "f64" & getNameString(MenuID)
        Case "T_STR"
            getRegName = "u16" & getNameString(MenuID)
            
    End Select
    
    
    
End Function

Function getDeclareReg(DataFormat As String, MenuID As String)

    Dim DeclareName As String

    Select Case DataFormat
    
        Case "T_U16"
            DeclareName = "uint16_t " & getRegName(DataFormat, MenuID)
        Case "T_S16", "T_Q15"
            DeclareName = "int16_t " & getRegName(DataFormat, MenuID)
        Case "T_U32"
            DeclareName = "uint32_t " & getRegName(DataFormat, MenuID)
        Case "T_S32"
           DeclareName = "int32_t " & getRegName(DataFormat, MenuID)
        Case "T_F32"
            DeclareName = "float32_t " & getRegName(DataFormat, MenuID)
        Case "T_D64"
            DeclareName = "float64_t " & getRegName(DataFormat, MenuID)
        Case "T_STR"
            DeclareName = "uint16_t " & getRegName(DataFormat, MenuID)
            
    End Select
    
    getDeclareReg = DeclareName
End Function

Sub printFileHeader(fs As Object, fileName As String)
    fs.write "/*" & vbCrLf & _
             " *  File Name: " & fileName & vbCrLf & _
             " *" & vbCrLf & _
             " *  Created on: " & Date & vbCrLf & _
             " *  Author: " & GetComputerName() & vbCrLf & _
             " */" & vbCrLf & vbCrLf
End Sub

Sub GenerateArraysByClass()

    Dim fd As Object, ws As Worksheet
    Dim classColumn As Range, cell As Range
    Dim arrDict As Object, arrCollection As Collection
    Dim className As Variant, arr() As Variant
    Dim sumofBytes As Integer, i As Integer, lastRow As Long
    
    ' Set worksheet and find the class column
    Set ws = ThisWorkbook.Worksheets("Menu")
    Set classColumn = ws.Rows(2).Find("Eeprom Group", LookIn:=xlValues, LookAt:=xlWhole)
    
    If classColumn Is Nothing Then
        MsgBox "Column 'Eeprom Group' not found.", vbExclamation
        Exit Sub
    End If
    
    ' Get the last row in the class column and initialize dictionary
    lastRow = ws.Cells(ws.Rows.Count, classColumn.Column).End(xlUp).Row
    Set arrDict = CreateObject("Scripting.Dictionary")
    
    ' Populate dictionary with class names and corresponding values
    For Each cell In ws.Range(classColumn.Offset(1), ws.Cells(lastRow, classColumn.Column))
        className = Trim(cell.Value)
        If className <> "" Then
            If Not arrDict.Exists(className) Then Set arrDict(className) = New Collection
            arrDict(className).Add Array(cell.Offset(0, -2).Value, cell.Offset(0, -5).Value)
        End If
    Next cell
    
    ' Create the output file
    Set fd = CreateObject("Scripting.FileSystemObject").CreateTextFile(Application.ActiveWorkbook.Path & "\" & "linkEeprom.h", True)
    
    ' Write the header and structure definitions
    printFileHeader fd, "linkEeprom.h"
    fd.write "typedef struct {" & vbCrLf & _
             "    uint16_t size;" & vbCrLf & _
             "    void *ptr;" & vbCrLf & _
             "} EE_REG;" & vbCrLf & _
             "typedef EE_REG * HAL_EEREG;" & vbCrLf & vbCrLf
    
    ' Exit if only the header exists
    If arrDict.Count = 1 Then
        fd.write "#define _TABLE_ADVPARAMS    5" & vbCrLf
        fd.Close
        Exit Sub
    End If
    
    ' Generate C code for each array based on class name
    For Each className In arrDict.Keys
        sumofBytes = 0
        Set arrCollection = arrDict(className)
        ReDim arr(1 To arrCollection.Count, 1 To 2)
        
        ' Populate the array and calculate the total bytes
        For i = 1 To arrCollection.Count
            arr(i, 1) = arrCollection(i)(0)
            arr(i, 2) = arrCollection(i)(1)
            sumofBytes = sumofBytes + arr(i, 2) * 2
        Next i
        sumofBytes = sumofBytes + 1  ' Add one byte for checksum
        
        ' Write block size and initialize the block array
        fd.write "#define _BLK16_" & UCase(className) & vbTab & sumofBytes & vbCrLf & _
                 "uint16_t blk" & className & "[_BLK16_" & UCase(className) & "];" & vbCrLf
        
        ' Generate and write C structure array for the current class
        fd.write "const EE_REG reg" & className & "[" & arrCollection.Count + 1 & "] = {" & vbCrLf
        For i = 1 To arrCollection.Count
            fd.write "              {" & arr(i, 2) * 2 & ", (void*)&" & arr(i, 1) & "}," & vbCrLf
        Next i
        fd.write "              {1, (void*)&blk" & className & "[" & (sumofBytes - 1) & "]}};" & vbCrLf & _
                 "#define _TABLE_" & UCase(className) & vbTab & arrCollection.Count + 1 & vbCrLf & vbCrLf
    Next className
    
    fd.Close
    MsgBox "File generated successfully at " & Application.ActiveWorkbook.Path, vbInformation
End Sub

'csvFilePath = Application.ThisWorkbook.Path & "\output_selected_columns_" & Format(Now, "yyyymmdd_hhmmss") & ".csv"


Sub ExportSelectedColumnsToCSV()
    Dim ws As Worksheet
    Dim csvFilePath As String
    Dim rowCount As Long, currentRow As Long
    Dim csvLine As String
    Dim fileNum As Integer
    Dim colID As Long, colFunc As Long, colFormat As Long
    Dim groupRow As Long
    
    ' Set worksheet
    Set ws = ThisWorkbook.Sheets("Menu")
    
    ' Find "Group" column header
    On Error Resume Next
    groupRow = Application.Match("Group", ws.Range("A:A"), 0)
    On Error GoTo 0
    If groupRow = 0 Then
        MsgBox "Group column not found", vbExclamation
        Exit Sub
    End If
    
    ' Set output CSV file path, create unique filename
    csvFilePath = Application.ThisWorkbook.Path & "\BusPollScript_ID_3.csv"
    
    ' Get the last column
    colID = Application.Match("ID", ws.Rows(groupRow), 0)
    colFunc = Application.Match("Menu", ws.Rows(groupRow), 0)
    colFormat = Application.Match("Format", ws.Rows(groupRow), 0)
    If IsError(colID) Or IsError(colFunc) Or IsError(colFormat) Then
        MsgBox "Required columns (ID, Menu, Format) not found", vbExclamation
        Exit Sub
    End If

    ' Open CSV file for writing
    fileNum = FreeFile
    Open csvFilePath For Output As fileNum
    
    ' Write CSV header
    Print #fileNum, "id,func,data,len,type,gain,offset,max,min"
    
    ' Get the number of rows
    rowCount = ws.Cells(ws.Rows.Count, colID).End(xlUp).Row
    
    ' Process each row
    For currentRow = groupRow + 1 To rowCount
        If Not IsEmpty(ws.Cells(currentRow, colID)) And Not IsEmpty(ws.Cells(currentRow, colFunc)) Then
            csvLine = ws.Cells(currentRow, colID).Value & "," & _
                      ws.Cells(currentRow, colFunc).Value & "," & _
                      GetDataValue(ws, currentRow) & "," & _
                      FormatConversion(ws.Cells(currentRow, colFormat).Value) & "," & _
                      "Dec," & _
                      GetDataGain(ws, currentRow) & "," & _
                      ",,"  ' Placeholder for 'Dec', empty max min
            Print #fileNum, csvLine
        End If
    Next currentRow

    ' Close file
    Close fileNum

    ' Show message
    MsgBox "CSV file exported to: " & csvFilePath
End Sub

Function FormatConversion(formatVal As String) As String
    Select Case formatVal
        Case "T_U16": FormatConversion = "u16"
        Case "T_F32": FormatConversion = "f32"
        Case "T_Q15": FormatConversion = "q15"
        Case "T_S16": FormatConversion = "s16"
        Case "T_U32": FormatConversion = "u32"
        Case "T_S32": FormatConversion = "s32"
        Case "T_D64": FormatConversion = "d64"
        Case "T_STR": FormatConversion = "str"
        Case "T_FUNC": FormatConversion = "func"
        Case Else: FormatConversion = formatVal
    End Select
End Function

Function GetDataValue(ws As Worksheet, rowNum As Long) As String
    ' Assume Data column is the 5th column
    GetDataValue = ws.Cells(rowNum, 5).Value ' Assume Data column is column B
End Function

Function GetDataGain(ws As Worksheet, rowNum As Long) As String
    ' Assume Data column is the 5th column
    GetDataGain = ws.Cells(rowNum, 8).Value ' Assume Data column is column B
End Function




Private Sub BuildButton_Click()

Dim fdTbMenu As Object, fdHMenu As Object, fdDefMenu As Object, fs As Object, fdlinkVar As Object

Dim GroupList As dCell
Dim GroupName As Range
Dim strGroup As String
Dim gpindex As Long

Dim MenuID As dCell
Dim Comment As dCell
Dim CmdID As dCell
Dim DataFormat As dCell
Dim words As dCell
Dim Link As dCell
Dim InitValue As dCell
Dim Q15max As dCell
Dim EepromGroup As dCell

Dim strMenuDef As String
Dim chkFinish As Boolean
Dim strValueRange As String

Dim memIndex As Long

Dim i As Long
Dim j As Long
Dim k As Long
Dim n As Long

Dim state As Integer
state = 0

Do While state < 10
    Select Case state
        Case 0: ' Initialize variables and find worksheet elements
            Set GroupList.ptr = CmdTable.Range("A:A").Find("Group")
            Set MenuID.ptr = CmdTable.Range(GroupList.ptr.Address & ":Z" & GroupList.ptr.Row).Find("Menu")
            Set Comment.ptr = CmdTable.Range(GroupList.ptr.Address & ":Z" & GroupList.ptr.Row).Find("Comment")
            Set CmdID.ptr = CmdTable.Range(GroupList.ptr.Address & ":Z" & GroupList.ptr.Row).Find("ID")
            Set DataFormat.ptr = CmdTable.Range(GroupList.ptr.Address & ":Z" & GroupList.ptr.Row).Find("Format")
            Set words.ptr = CmdTable.Range(GroupList.ptr.Address & ":Z" & GroupList.ptr.Row).Find("Words")
            Set Link.ptr = CmdTable.Range(GroupList.ptr.Address & ":Z" & GroupList.ptr.Row).Find("Link")
            Set InitValue.ptr = CmdTable.Range(GroupList.ptr.Address & ":Z" & GroupList.ptr.Row).Find("Initial Value")
            Set Q15max.ptr = CmdTable.Range(GroupList.ptr.Address & ":Z" & GroupList.ptr.Row).Find("Q15 Maximum")
            Set EepromGroup.ptr = CmdTable.Range(GroupList.ptr.Address & ":Z" & GroupList.ptr.Row).Find("Eeprom Group")
            
            ExportSelectedColumnsToCSV
            memIndex = 0
            state = 1
        
        Case 1: ' Create Group List Range
            GroupList.str = ""
            For Each GroupName In CmdTable.UsedRange
                If GroupName.Column = GroupList.ptr.Column Then
                    If "" <> GroupName.Text Then
                        If True = IsNumeric(GroupName.Text) Then
                            If GroupList.ptr.Text <> GroupName.Text Then
                                If "" = GroupList.str Then
                                    GroupList.str = GroupName.Address
                                Else
                                    GroupList.str = GroupList.str + "," + GroupName.Address
                                End If
                            End If
                        End If
                    End If
                End If
            Next GroupName
            state = 2
        
        Case 2: ' Calculate Words
            For Each GroupName In CmdTable.Range(GroupList.str)
                gpindex = CmdTable.Cells(GroupName.Row, GroupName.Column)
                strGroup = CmdTable.Cells(GroupName.Row, GroupName.Column)
                MenuID.str = CmdTable.Cells(GroupName.Row, MenuID.ptr.Column)
                i = 0
                k = 0
                Do
                    Select Case CmdTable.Cells(GroupName.Row + i, DataFormat.ptr.Column)
                        Case "T_U16", "T_S16", "T_U32", "T_S32", "T_F32", "T_D64", "T_Q15"
                            CmdTable.Cells(GroupName.Row + i, words.ptr.Column) = CmdType.Cells(CmdType.Range("A:A").Find(CmdTable.Cells(GroupName.Row + i, DataFormat.ptr.Column)).Row, 2)
                        Case Else
                            ' Skip this cell for filling the length
                    End Select
                    memIndex = memIndex + CmdTable.Cells(GroupName.Row + i, words.ptr.Column)
                    If (1 < CmdTable.Cells(GroupName.Row + i, words.ptr.Column)) And (memIndex Mod 2) Then
                        MsgBox "_mu" & getNameString(MenuID.str) & " can not be divided by 2."
                        Stop
                    End If
                    i = i + 1
                    strGroup = CmdTable.Cells(GroupName.Row + i, GroupName.Column)
                    MenuID.str = CmdTable.Cells(GroupName.Row + i, MenuID.ptr.Column)
                    chkFinish = (0 < Len(strGroup)) Or (0 = Len(MenuID.str))
                Loop Until chkFinish
            Next GroupName
            state = 3
        
        Case 3: ' Create tbmenu.c
            Set fs = CreateObject("Scripting.FileSystemObject")
            Set fdTbMenu = fs.CreateTextFile(Application.ActiveWorkbook.Path & "\" & "tbmenu.c", True)
            With fdTbMenu
                printFileHeader fdTbMenu, "tbmenu.c"
                .write "#include ""ModbusSlave.h""" & vbCrLf
                .write "#include ""mbcmd.h""" & vbCrLf
                .write vbCrLf & vbCrLf
                .write "REG_MBUSDATA regMbusData;" & vbCrLf & vbCrLf
                .write "int chkValidAddress(uint16_t addr) {" & vbCrLf
                .write "    if (addr < _size_of_mbslave_id) {" & vbCrLf
                .write "        return 0;" & vbCrLf
                .write "    } else {" & vbCrLf
                .write "        return MB_ERROR_ILLEGALADDR;" & vbCrLf
                .write "    }" & vbCrLf
                .write "}" & vbCrLf & vbCrLf
                .write "uint16_t getModbusData(uint16_t addr) {" & vbCrLf
                .write "    if (addr < _size_of_mbslave_id) {" & vbCrLf
                .write "        return regMbusData.u16MbusData[addr];" & vbCrLf
                .write "    } else {" & vbCrLf
                .write "        return 0xFFFF;" & vbCrLf
                .write "    }" & vbCrLf
                .write "}" & vbCrLf & vbCrLf
                .write "uint16_t setModbusData(uint16_t addr, uint16_t data) {" & vbCrLf
                .write "    if (addr < _size_of_mbslave_id) {" & vbCrLf
                .write "        regMbusData.u16MbusData[addr] = data;" & vbCrLf
                .write "        return data;" & vbCrLf
                .write "    } else {" & vbCrLf
                .write "        return 0xFFFF;" & vbCrLf
                .write "    }" & vbCrLf
                .write "}" & vbCrLf & vbCrLf
                .write vbCrLf & vbCrLf
                .Close
            End With
            state = 4
        
        Case 4: ' Create mbcmd.h
            Set fdHMenu = fs.CreateTextFile(Application.ActiveWorkbook.Path & "\" & "mbcmd.h", True)
            With fdHMenu
                printFileHeader fdHMenu, "mbcmd.h"
                .write "#ifndef MBCMD_H_" & vbCrLf
                .write "#define MBCMD_H_" & vbCrLf & vbCrLf
                .write "typedef enum {" & vbCrLf
                i = 1
                For Each GroupName In CmdTable.Range(GroupList.str)
                    .write vbTab & "_g" & UCase(GroupName.Text) & "_MODE"
                    If 1 = i Then
                        .write " = 0" & "," & vbCrLf
                    Else
                        .write "," & vbCrLf
                    End If
                    i = i + 1
                Next GroupName
                .write "    _END_OF_MODE" & vbCrLf
                .write "} ID_MODE;" & vbCrLf
                .write vbCrLf & vbCrLf
                For Each GroupName In CmdTable.Range(GroupList.str)
                    .write "enum {" & vbCrLf
                    gpindex = CmdTable.Cells(GroupName.Row, GroupName.Column)
                    strGroup = CmdTable.Cells(GroupName.Row, GroupName.Column)
                    MenuID.str = CmdTable.Cells(GroupName.Row, MenuID.ptr.Column)
                    i = 0
                    k = 0
                    n = 0
                    Do
                        NumberOfWord = CmdTable.Cells(GroupName.Row + i, words.ptr.Column)
                        IndexOfWord = 1
                        For IndexOfWord = 1 To NumberOfWord
                            ArraryIndex = IndexOfWord - 1
                            If 1 < NumberOfWord Then
                                IndexString = Replace(str(ArraryIndex), " ", "")
                            Else
                                IndexString = ""
                            End If
                            strMenuDef = fillSpace("_mu" & getNameString(MenuID.str) & IndexString & " = " & (gpindex + k + ArraryIndex) & ",", 40)
                            strValueRange = "// #" & (gpindex + k + ArraryIndex) & "  " & CmdTable.Cells(GroupName.Row + i, DataFormat.ptr.Column)
                            strValueRange = fillSpace(strValueRange, 24)
                            strMenuDef = strMenuDef & strValueRange & "    " & CmdTable.Cells(GroupName.Row + i, Comment.ptr.Column)
                            .write vbTab & strMenuDef & vbCrLf
                        Next IndexOfWord
                        n = n + 1
                        k = k + CmdTable.Cells(GroupName.Row + i, words.ptr.Column)
                        i = i + 1
                        strGroup = CmdTable.Cells(GroupName.Row + i, GroupName.Column)
                        MenuID.str = CmdTable.Cells(GroupName.Row + i, MenuID.ptr.Column)
                        chkFinish = (0 < Len(strGroup)) Or (0 = Len(MenuID.str))
                    Loop Until chkFinish
                    .write "    _size_of_mbslave_id" & vbCrLf
                    .write "};" & vbCrLf & vbCrLf
                Next GroupName
                .write "typedef union { " & vbCrLf
                .write "    uint16_t u16MbusData[_size_of_mbslave_id];" & vbCrLf
                .write "    struct { " & vbCrLf
                For Each GroupName In CmdTable.Range(GroupList.str)
                    strGroup = CmdTable.Cells(GroupName.Row, GroupName.Column)
                    MenuID.str = CmdTable.Cells(GroupName.Row, MenuID.ptr.Column)
                    i = 0
                    k = 0
                    n = 0
                    Do
                        Select Case CmdTable.Cells(GroupName.Row + i, DataFormat.ptr.Column)
                            Case "T_U16", "T_S16", "T_Q15", "T_U32", "T_S32", "T_F32", "T_D64"
                                .write vbTab & vbTab & getDeclareReg(CmdTable.Cells(GroupName.Row + i, DataFormat.ptr.Column), MenuID.str) & ";" & vbCrLf
                            Case "T_STR"
                                .write vbTab & vbTab & getDeclareReg(CmdTable.Cells(GroupName.Row + i, DataFormat.ptr.Column), MenuID.str) & "[" & CmdTable.Cells(GroupName.Row + i, words.ptr.Column) & "];" & vbCrLf
                        End Select
                        CmdTable.Cells(GroupName.Row + i, CmdID.ptr.Column) = k + CmdTable.Cells(GroupName.Row, GroupName.Column)
                        If "" = Replace(CmdTable.Cells(GroupName.Row + i, words.ptr.Column), " ", "") Then CmdTable.Cells(GroupName.Row + i, words.ptr.Column) = 1
                        k = k + CmdTable.Cells(GroupName.Row + i, words.ptr.Column)
                        i = i + 1
                        strGroup = CmdTable.Cells(GroupName.Row + i, GroupName.Column)
                        MenuID.str = CmdTable.Cells(GroupName.Row + i, MenuID.ptr.Column)
                        chkFinish = (0 < Len(strGroup)) Or (0 = Len(MenuID.str))
                    Loop Until chkFinish
                Next GroupName
                .write "    }; " & vbCrLf
                .write "} REG_MBUSDATA;" & vbCrLf
                .write "extern REG_MBUSDATA regMbusData;" & vbCrLf
                .write "extern int chkValidAddress(uint16_t addr);" & vbCrLf
                .write "extern uint16_t getModbusData(uint16_t addr);" & vbCrLf
                .write "extern uint16_t setModbusData(uint16_t addr, uint16_t data);" & vbCrLf
                .write vbCrLf & vbCrLf
                .write vbCrLf & vbCrLf & "#endif /* MBCMD_H_ */"
                .write vbCrLf & vbCrLf
                .Close
            End With
            state = 5
        
        Case 5: ' Create linkVariables.c
            Set fdlinkVar = fs.CreateTextFile(Application.ActiveWorkbook.Path & "\" & "linkVariables.c", True)
            With fdlinkVar
                printFileHeader fdlinkVar, "linkVariables.c"
                .write "#include ""ModbusCommon.h""" & vbCrLf
                .write "#include ""ModbusSlave.h""" & vbCrLf
                .write vbCrLf & vbCrLf
                .write "void initRegN(void *v){ " & vbCrLf & vbCrLf
                .write vbTab & "SCI_MODBUS *p = (SCI_MODBUS *) v;" & vbCrLf
                k = 0
                For Each GroupName In CmdTable.Range(GroupList.str)
                    strGroup = CmdTable.Cells(GroupName.Row, GroupName.Column)
                    MenuID.str = CmdTable.Cells(GroupName.Row, MenuID.ptr.Column)
                    i = 0
                    Do
                        If "" <> CmdTable.Cells(GroupName.Row + i, InitValue.ptr.Column) Then
                            If "" <> CmdTable.Cells(GroupName.Row + i, Link.ptr.Column) Then
                                TmpStr = CmdTable.Cells(GroupName.Row + i, Link.ptr.Column) & " = "
                                Select Case CmdTable.Cells(GroupName.Row + i, DataFormat.ptr.Column)
                                    Case "T_U16", "T_S16", "T_U32", "T_S32", "T_F32", "T_D64"
                                        TmpStr = TmpStr & CmdTable.Cells(GroupName.Row + i, InitValue.ptr.Column) & "; " & vbCrLf
                                    Case "T_Q15"
                                        If 0 < CmdTable.Cells(GroupName.Row + i, Q15max.ptr.Column) Then
                                            TmpStr = TmpStr & (CmdTable.Cells(GroupName.Row + i, InitValue.ptr.Column) / CmdTable.Cells(GroupName.Row + i, Q15max.ptr.Column)) & "; " & vbCrLf
                                        Else
                                            TmpStr = TmpStr & "0; " & vbCrLf
                                            MsgBox "_mu" & getNameString(MenuID.str) & " divided by 0"
                                        End If
                                End Select
                            Else
                                TmpStr = "p->pReg->"
                                Select Case CmdTable.Cells(GroupName.Row + i, DataFormat.ptr.Column)
                                    Case "T_U16", "T_S16", "T_U32", "T_S32", "T_F32", "T_D64"
                                        TmpStr = TmpStr & getRegName(CmdTable.Cells(GroupName.Row + i, DataFormat.ptr.Column), MenuID.str) & " = " & CmdTable.Cells(GroupName.Row + i, InitValue.ptr.Column) & "; " & vbCrLf
                                    Case "T_Q15"
                                        TmpStr = TmpStr & getRegName(CmdTable.Cells(GroupName.Row + i, DataFormat.ptr.Column), MenuID.str) & " = MB_SFtoQ15(" & CmdTable.Cells(GroupName.Row + i, InitValue.ptr.Column) & "); " & vbCrLf
                                End Select
                            End If
                            TmpStr = vbTab & TmpStr
                            .write TmpStr
                        End If
                        i = i + 1
                        strGroup = CmdTable.Cells(GroupName.Row + i, GroupName.Column)
                        MenuID.str = CmdTable.Cells(GroupName.Row + i, MenuID.ptr.Column)
                        chkFinish = (0 < Len(strGroup)) Or (0 = Len(MenuID.str))
                    Loop Until chkFinish
                    k = k + 1
                Next GroupName
                .write "}" & vbCrLf & vbCrLf
                .write "void readRegN(void *v){ " & vbCrLf & vbCrLf
                .write vbTab & "SCI_MODBUS *p = (SCI_MODBUS *) v;" & vbCrLf
                k = 0
                For Each GroupName In CmdTable.Range(GroupList.str)
                    strGroup = CmdTable.Cells(GroupName.Row, GroupName.Column)
                    MenuID.str = CmdTable.Cells(GroupName.Row, MenuID.ptr.Column)
                    i = 0
                    Do
                        If "" <> CmdTable.Cells(GroupName.Row + i, Link.ptr.Column) Then
                            TmpStr = "p->pReg->"
                            Select Case CmdTable.Cells(GroupName.Row + i, DataFormat.ptr.Column)
                                Case "T_U16", "T_S16", "T_U32", "T_S32", "T_F32", "T_D64"
                                    TmpStr = TmpStr & getRegName(CmdTable.Cells(GroupName.Row + i, DataFormat.ptr.Column), MenuID.str) & " = " & CmdTable.Cells(GroupName.Row + i, Link.ptr.Column) & "; " & vbCrLf
                                Case "T_Q15"
                                    TmpStr = TmpStr & getRegName(CmdTable.Cells(GroupName.Row + i, DataFormat.ptr.Column), MenuID.str) & " = MB_SFtoQ15(" & CmdTable.Cells(GroupName.Row + i, Link.ptr.Column) & "); " & vbCrLf
                            End Select
                            TmpStr = vbTab & TmpStr
                            .write TmpStr
                        End If
                        i = i + 1
                        strGroup = CmdTable.Cells(GroupName.Row + i, GroupName.Column)
                        MenuID.str = CmdTable.Cells(GroupName.Row + i, MenuID.ptr.Column)
                        chkFinish = (0 < Len(strGroup)) Or (0 = Len(MenuID.str))
                    Loop Until chkFinish
                    k = k + 1
                Next GroupName
                .write "}" & vbCrLf & vbCrLf
                .write "void writeReg(void *v){ " & vbCrLf & vbCrLf
                .write vbTab & "SCI_MODBUS *p = (SCI_MODBUS *) v;" & vbCrLf
                .write vbTab & "switch(p->info.rwfrom) {" & vbCrLf
                For Each GroupName In CmdTable.Range(GroupList.str)
                    strGroup = CmdTable.Cells(GroupName.Row, GroupName.Column)
                    MenuID.str = CmdTable.Cells(GroupName.Row, MenuID.ptr.Column)
                    i = 0
                    k = 0
                    n = 0
                    Do
                        If "" <> CmdTable.Cells(GroupName.Row + i, Link.ptr.Column) Then
                            NumberOfWord = CmdTable.Cells(GroupName.Row + i, words.ptr.Column)
                            If 1 < NumberOfWord Then
                                IndexString = "0"
                            Else
                                IndexString = ""
                            End If
                            TmpStr = "case _mu" & getNameString(MenuID.str) & IndexString & " :   " & CmdTable.Cells(GroupName.Row + i, Link.ptr.Column) & " = "
                            Select Case CmdTable.Cells(GroupName.Row + i, DataFormat.ptr.Column)
                                Case "T_U16", "T_S16", "T_U32", "T_S32", "T_F32", "T_D64"
                                    TmpStr = TmpStr & "p->pReg->" & getRegName(CmdTable.Cells(GroupName.Row + i, DataFormat.ptr.Column), MenuID.str) & "; break;" & vbCrLf
                                Case "T_Q15"
                                    TmpStr = TmpStr & "MB_Q15toSF(" & "p->pReg->" & getRegName(CmdTable.Cells(GroupName.Row + i, DataFormat.ptr.Column), MenuID.str) & "); break;" & vbCrLf
                            End Select
                            TmpStr = vbTab & TmpStr
                            .write TmpStr
                        End If
                        k = k + CmdTable.Cells(GroupName.Row + i, words.ptr.Column)
                        i = i + 1
                        strGroup = CmdTable.Cells(GroupName.Row + i, GroupName.Column)
                        MenuID.str = CmdTable.Cells(GroupName.Row + i, MenuID.ptr.Column)
                        chkFinish = (0 < Len(strGroup)) Or (0 = Len(MenuID.str))
                    Loop Until chkFinish
                Next GroupName
                .write vbTab & "default:" & vbCrLf
                .write vbTab & "    break;" & vbCrLf
                .write vbTab & "}" & vbCrLf
                .write "}" & vbCrLf & vbCrLf
                .write "void writeRegN(void *v){ " & vbCrLf & vbCrLf
                .write vbTab & "SCI_MODBUS *p = (SCI_MODBUS *) v;" & vbCrLf
                k = 0
                For Each GroupName In CmdTable.Range(GroupList.str)
                    strGroup = CmdTable.Cells(GroupName.Row, GroupName.Column)
                    MenuID.str = CmdTable.Cells(GroupName.Row, MenuID.ptr.Column)
                    i = 0
                    n = 0
                    Do
                        If "" <> CmdTable.Cells(GroupName.Row + i, Link.ptr.Column) Then
                            TmpStr = CmdTable.Cells(GroupName.Row + i, Link.ptr.Column) & " = "
                            Select Case CmdTable.Cells(GroupName.Row + i, DataFormat.ptr.Column)
                                Case "T_U16", "T_S16", "T_U32", "T_S32", "T_F32", "T_D64"
                                    TmpStr = TmpStr & "p->pReg->" & getRegName(CmdTable.Cells(GroupName.Row + i, DataFormat.ptr.Column), MenuID.str) & "; " & vbCrLf
                                Case "T_Q15"
                                    TmpStr = TmpStr & "MB_Q15toSF(" & "p->pReg->" & getRegName(CmdTable.Cells(GroupName.Row + i, DataFormat.ptr.Column), MenuID.str) & "); " & vbCrLf
                            End Select
                            TmpStr = vbTab & TmpStr
                            .write TmpStr
                            n = n + 1
                        End If
                        i = i + 1
                        strGroup = CmdTable.Cells(GroupName.Row + i, GroupName.Column)
                        MenuID.str = CmdTable.Cells(GroupName.Row + i, MenuID.ptr.Column)
                        chkFinish = (0 < Len(strGroup)) Or (0 = Len(MenuID.str))
                    Loop Until chkFinish
                    k = k + 1
                Next GroupName
                .write "}" & vbCrLf & vbCrLf
                .Close
            End With
            state = 6
        
        Case 6: ' Generate EEPROM arrays
            GenerateArraysByClass
            state = 7
        
        Case 7: ' End
            Exit Do
    End Select
Loop

End Sub





