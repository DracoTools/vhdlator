-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- A conversion between closely related array types of a value carried with
-- its bounds, joined to another: Microwatt's divider_tb writes
-- "std_ulogic_vector(resize(signed(rnd.RandSlv(dlength * 8)), 32)) &
-- x"00000000"", where a protected type's function gives a vector of a length
-- only running settles. The joining was emitted as one of fixed vectors, the
-- carrier taken for its bits, which the C++ compiler refused.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
package t_vhdl_dynconvcat_pkg is
  type gen_t is protected
    impure function RandSlv(n : natural) return std_logic_vector;
  end protected;
end package;
package body t_vhdl_dynconvcat_pkg is
  type gen_t is protected body
    variable k : integer := 0;
    impure function RandSlv(n : natural) return std_logic_vector is
      variable v : std_logic_vector(n - 1 downto 0);
    begin
      k := k + 1;
      for i in v'range loop
        if ((i + k) mod 3) = 0 then v(i) := '1'; else v(i) := '0'; end if;
      end loop;
      return v;
    end function;
  end protected body;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.t_vhdl_dynconvcat_pkg.all;
entity t_vhdl_dynconvcat is end;
architecture t of t_vhdl_dynconvcat is
begin
  process
    variable rnd : gen_t;
    variable ra, rb : std_ulogic_vector(63 downto 0);
  begin
    for dlength in 1 to 3 loop
      ra := std_ulogic_vector(resize(signed(rnd.RandSlv(dlength * 8)), 32)) & x"00000000";
      rb := std_ulogic_vector(resize(signed(rnd.RandSlv(dlength * 8)), 64));
      report "ra=" & to_hstring(ra) & " rb=" & to_hstring(rb);
      case dlength is
        when 1 => assert ra = x"0000002400000000" and rb = x"FFFFFFFFFFFFFF92" severity failure;
        when 2 => assert ra = x"FFFF924900000000" and rb = x"0000000000004924" severity failure;
        when others => assert ra = x"0049249200000000" and rb = x"0000000000249249" severity failure;
      end case;
    end loop;
    report "*-* All Finished *-*";
    wait;
  end process;
end;
