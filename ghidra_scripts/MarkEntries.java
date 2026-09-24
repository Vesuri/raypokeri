// Mark entry points from the sibling entrypoints.csv (addr,name,note) and disassemble
// from each, so auto-analysis can follow control flow into code it cannot reach on its
// own (vector/TRAP targets, interrupt handlers, jump-table entries).  Flat address space:
// the program ROM image is imported at $00000.
//@category Pokeri
import java.io.BufferedReader;
import java.io.InputStreamReader;
import ghidra.app.script.GhidraScript;
import generic.jar.ResourceFile;
import ghidra.program.model.address.Address;
import ghidra.program.model.symbol.SourceType;

public class MarkEntries extends GhidraScript {
    @Override
    public void run() throws Exception {
        ResourceFile csv = new ResourceFile(getSourceFile().getParentFile(), "entrypoints.csv");
        if (!csv.exists()) {
            printerr("MarkEntries: entrypoints.csv not found next to the script: " + csv);
            return;
        }
        int marked = 0, skipped = 0;
        try (BufferedReader r = new BufferedReader(new InputStreamReader(csv.getInputStream()))) {
            String line;
            while ((line = r.readLine()) != null) {
                line = line.trim();
                if (line.isEmpty() || line.startsWith("#") || line.startsWith("addr,")) continue;
                String[] parts = line.split(",", 3);
                String addrStr = parts[0].trim().replaceAll("^\\$", "").replaceAll("^0[xX]", "");
                String name = parts.length > 1 ? parts[1].trim() : "";
                Address a;
                try { a = toAddr(Long.parseLong(addrStr, 16)); }
                catch (NumberFormatException e) { skipped++; continue; }
                if (!name.isEmpty()) createLabel(a, name, true, SourceType.USER_DEFINED);
                disassemble(a);
                if (getFunctionAt(a) == null) createFunction(a, name.isEmpty() ? null : name);
                marked++;
            }
        }
        println("MarkEntries: marked=" + marked + " skipped=" + skipped);
    }
}
