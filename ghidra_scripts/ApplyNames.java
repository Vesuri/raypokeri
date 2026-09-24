// Apply names and plate comments from disasm/symbols.csv (the name source of truth).
// Columns: addr,name,type,evidence,note — flat address space.  Arg0 = path to symbols.csv.
//@category Pokeri
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import java.io.*;

public class ApplyNames extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        String csvPath = args.length > 0 ? args[0] : "disasm/symbols.csv";
        int applied = 0, skipped = 0;
        BufferedReader r = new BufferedReader(new FileReader(csvPath));
        String line;
        while ((line = r.readLine()) != null) {
            line = line.trim();
            if (line.isEmpty() || line.startsWith("#") || line.startsWith("addr,")) continue;
            String[] parts = line.split(",", 5);
            if (parts.length < 5) { skipped++; continue; }
            // Accept "$0450", "0x0450" and bare "0450".
            String addrStr = parts[0].trim().replaceAll("^\\$", "").replaceAll("^0[xX]", "");
            String name = parts[1].trim();
            String note = "[" + parts[3].trim() + "] " + parts[4].trim();
            long offset;
            try { offset = Long.parseLong(addrStr, 16); }
            catch (NumberFormatException e) { skipped++; continue; }
            Address a = toAddr(offset);
            try {
                if (!name.isEmpty()) {
                    Symbol existing = getSymbolAt(a);
                    if (existing == null || existing.getSource() != SourceType.USER_DEFINED)
                        createLabel(a, name, true, SourceType.USER_DEFINED);
                }
                CodeUnit cu = currentProgram.getListing().getCodeUnitAt(a);
                if (cu != null) {
                    String cur = cu.getComment(CodeUnit.PLATE_COMMENT);
                    if (cur == null || cur.isEmpty()) cu.setComment(CodeUnit.PLATE_COMMENT, note);
                }
                applied++;
            } catch (Exception e) {
                println("SKIP " + addrStr + " (" + name + "): " + e.getMessage());
                skipped++;
            }
        }
        r.close();
        println("ApplyNames: applied=" + applied + " skipped=" + skipped);
    }
}
