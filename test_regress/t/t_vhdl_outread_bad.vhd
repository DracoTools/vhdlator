-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A port of mode out cannot be read before VHDL-2008, and these attributes
-- read it: they answer about the value the signal has or had. 'DRIVING_VALUE
-- asks about what this process drives, which an output is exactly for.

entity t is
  port ( signal o : out bit;
         signal i : in  bit );
end entity;

architecture rtl of t is
begin
  process
  begin
    if i'event then                     -- an input may be read
      null;
    end if;
    if o'driving_value = '1' then       -- what this process drives, which is allowed
      null;
    end if;
    if o'event then                     -- reading the output
      null;
    end if;
    if o'stable then                    -- and again
      null;
    end if;
    wait;
  end process;
end architecture;
