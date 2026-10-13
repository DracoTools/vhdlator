-- DESCRIPTION: Verilator: VHDL-93 textio writes a BOOLEAN in capitals
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

-- Before VHDL-2008, WRITE of a BOOLEAN gives "TRUE" and "FALSE", as GHDL and
-- Questa give them; 'IMAGE is in lower case either way (GHDL gna issue2115)

use std.textio.all;

entity t_vhdl_bool93 is
end entity;

architecture a of t_vhdl_bool93 is
begin
  process
    variable l : line;
  begin
    write(l, false);
    assert l.all = "FALSE" severity failure;
    deallocate(l);
    write(l, true);
    assert l.all = "TRUE" severity failure;
    assert boolean'image(true) = "true" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
