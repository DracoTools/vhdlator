-- DESCRIPTION: Verilator: VHDL process (all) reading a signal in a local procedure
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

-- A subprogram the process declares is part of the process statement, so
-- what it reads is in the implied sensitivity list (11.3; GHDL gna bug0129)

entity t_vhdl_allproc is
end entity;

architecture a of t_vhdl_allproc is
  signal en : bit := '1';
  signal cnt, cnt_next : natural := 0;
begin
  p_main : process (all)
    procedure count(constant r : in natural; variable v : out natural) is
    begin
      if en = '1' then
        v := r + 1;
      else
        v := r;
      end if;
    end procedure;
    variable v : natural;
  begin
    count(cnt, v);
    cnt_next <= v;
  end process;

  process
  begin
    cnt <= 1;
    wait for 1 ns;
    assert cnt_next = 2 severity failure;
    en <= '0';
    wait for 1 ns;
    assert cnt_next = 1 severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
