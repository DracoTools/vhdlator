-- DESCRIPTION: Verilator: VHDL run-time check: writing a file opened for reading (NVC file13)
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

entity t_vhdl_filewr_mode_bad is
end entity;

architecture a of t_vhdl_filewr_mode_bad is
  type ft is file of character;
  file f : ft;
begin
  process is
    variable c : character;
  begin
    file_open(f, "t_vhdl_filewr_mode_bad.tmp", write_mode);
    write(f, 'x');
    file_close(f);
    file_open(f, "t_vhdl_filewr_mode_bad.tmp", read_mode);
    read(f, c);
    assert c = 'x' severity failure;
    report "read back";
    write(f, 'y');
    report "not reached" severity failure;
    wait;
  end process;
end architecture;
