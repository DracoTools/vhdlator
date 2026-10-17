-- DESCRIPTION: Verilator: VHDL run-time check: reading a file opened for writing (NVC file14)
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

entity t_vhdl_filerd_mode_bad is
end entity;

architecture a of t_vhdl_filerd_mode_bad is
  type ft is file of character;
  file f : ft;
begin
  process is
    variable c : character;
  begin
    file_open(f, "t_vhdl_filerd_mode_bad.tmp", write_mode);
    write(f, 'x');
    report "written";
    read(f, c);
    report "not reached" severity failure;
    wait;
  end process;
end architecture;
