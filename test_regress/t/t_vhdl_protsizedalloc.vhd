-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- "new std_logic_vector'(SlvVal)" with SlvVal a constant of a method, sized
-- by a parameter, "std_logic_vector(Size - 1 downto 0)": a value that carries
-- its bounds (9.3.7). OSVVM's ModelParametersSingletonPkg has such a Set that
-- nothing calls, which is emitted as it was written, not copied for a call.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

package t_vhdl_protsizedalloc_pkg is
  type st_t is protected
    procedure set(Data : in integer; Size : positive);
    procedure set_uncalled(Data : in integer; Size : positive);
    procedure set_slv(Data : in std_logic_vector);
    impure function get return std_logic_vector;
  end protected;
end package;

package body t_vhdl_protsizedalloc_pkg is
  type st_t is protected body
    type SlvPtrType is access std_logic_vector;
    variable p : SlvPtrType;
    procedure set(Data : in integer; Size : positive) is
      constant SlvVal : std_logic_vector(Size - 1 downto 0)
        := std_logic_vector(to_signed(Data, Size));
    begin
      p := new std_logic_vector'(SlvVal);
    end procedure;
    procedure set_uncalled(Data : in integer; Size : positive) is
      constant SlvVal : std_logic_vector(Size - 1 downto 0)
        := std_logic_vector(to_signed(Data, Size));
    begin
      p := new std_logic_vector'(SlvVal);
    end procedure;
    procedure set_slv(Data : in std_logic_vector) is
    begin
      p := new std_logic_vector'(Data);
    end procedure;
    impure function get return std_logic_vector is
    begin
      return p.all;
    end function;
  end protected body;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_protsizedalloc_pkg.all;

entity t_vhdl_protsizedalloc is
end entity;

architecture t of t_vhdl_protsizedalloc is
  shared variable st : st_t;
begin
  process
  begin
    st.set(-3, 6);
    assert st.get = "111101" severity failure;
    st.set(5, 10);
    assert st.get = "0000000101" severity failure;
    st.set_slv("1010");
    assert st.get = "1010" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
