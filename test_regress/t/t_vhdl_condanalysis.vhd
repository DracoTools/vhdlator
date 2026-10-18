-- DESCRIPTION: Verilator: VHDL test
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- Conditional analysis (VHDL-2019 24.2), which nvc 1.22.1 also takes in
-- VHDL-2008: only the text of the branch whose condition holds is analyzed.
-- The standard identifiers, one the command line defines, nesting, and the
-- logical operators. Run under VHDL-2019 (t_vhdl_condanalysis) and VHDL-2008
-- (t_vhdl_condanalysis08), with -DUSER_ID=yes.

entity t_vhdl_condanalysis is
end entity;

architecture a of t_vhdl_condanalysis is
`if VHDL_VERSION = "2019" then
  constant ver : integer := 2019;
`elsif VHDL_VERSION = "2008" then
  constant ver : integer := 2008;
`else
  constant ver : integer := 0;
`end if
begin
  process
    variable hits : natural := 0;
  begin
`if TOOL_TYPE = "SIMULATION" and not (TOOL_NAME = "nobody") then
    hits := hits + 1;
`if USER_ID = "yes" then
    hits := hits + 1;
`else
    this is not analyzed, so it need not be VHDL;
`end
`elsif TOOL_TYPE = "SYNTHESIS" then
    hits := 100;
`end if
`if VHDL_VERSION >= "2019" or VHDL_VERSION < "2000" then
    assert ver = 2019 severity failure;
`else
    assert ver = 2008 severity failure;
`end
    assert hits = 2 severity failure;
    report "version " & integer'image(ver);
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
