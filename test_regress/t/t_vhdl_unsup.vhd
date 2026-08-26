-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- Constructs the frontend understands but cannot yet lower. Each must be
-- reported as unsupported rather than mis-compiled, and must name what it is
-- so the message is useful.

entity t_vhdl_unsup is
  port (clk : in bit;
        q   : out bit_vector(7 downto 0));
end entity;

architecture rtl of t_vhdl_unsup is
  type memory_t is file of character;
  file log : memory_t;
  signal v : bit_vector(7 downto 0);
begin

  -- A process that waits needs the suspendable processes of --timing
  waiter : process is
  begin
    v <= "00000001";
    wait for 10 ns;
  end process;

  -- An external name that starts from the enclosing region rather than the
  -- design root has no equivalent to point at yet (8.7)
  q(0) <= << signal ^.outer : bit >>;

end architecture;
