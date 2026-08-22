-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- Syntax errors, and labels at 'end' that do not match their construct. The
-- parser must report each problem once and resynchronize, rather than turning
-- one mistake into a cascade.

-- An otherwise valid entity whose closing label is wrong
entity t is
  port (clk : in bit);
end entity wrongname;

-- A missing semicolon inside a port clause
entity u is
  port (clk : in bit
        rst : in bit);
end entity u;

architecture rtl of t is
  signal s : bit;
begin
  p : process (clk) is
  begin
    if s = '1' then
      s <= '0';
    end if;
  end process otherlabel;
end architecture rtl;
