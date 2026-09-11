-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A generic map names generics the package has (6.5.7.2). A named array
-- aggregate has a null range as a choice only where that is its single
-- choice (9.3.3.3). The index specification of a block configuration is
-- locally static (3.4.2). References: nvc 1.22.1, GHDL 7.0.0-dev, Questa
-- 2025.2.

package gen is
  generic (width : natural := 8);
end package;

package inst_good is new work.gen generic map (width => 4);
package inst_bad is new work.gen generic map (depth => 4);  -- Bad

entity t is
  port (n : integer);
end entity;

architecture a of t is
begin
  g : for i in 1 to 3 generate
  end generate;
  process
    type t0 is array (1 to 0) of integer;
    type t1 is array (1 to 1) of integer;
    variable v0 : t0;
    variable v : t1;
  begin
    v0 := (1 to 0 => 16);
    v := (1 to 0 => 16, 1 => 12);  -- Bad
    wait;
  end process;
end architecture;

configuration cfg of t is
  for a
    for g (1 to 2) end for;
    for g (1 to n) end for;  -- Bad: n is a port
  end for;
end configuration;
