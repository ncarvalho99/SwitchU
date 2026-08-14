#pragma once

#include <cstdint>
#include <list>
#include <string>
#include <functional>
#include <vector>

namespace themeshop::http {

bool initialize();
void shutdown();
bool isInitialized();

// Cancels small catalogue requests that are still in flight while the menu is
// handing control to a system applet.  Package downloads deliberately keep
// their own uninterrupted path through getToFile().
void cancelPendingRequests();

std::vector<std::uint8_t> getBytes(const std::string& url,
                                   const std::list<std::string>& headers = {});

// Grava a resposta direto em disco, sem passar pela memoria.
//
// getBytes acumula o corpo inteiro em um ostringstream e ainda o copia duas
// vezes ate virar vetor -- para um pacote de tema de 40 MB isso passa de 150 MB
// de pico, e o download morre com "Failed writing body". Serve para JSON, nao
// para arquivos grandes.
//
// Devolve o total de bytes escritos. Lanca em erro de HTTP ou de escrita.
std::uint64_t getToFile(const std::string& url,
                        const std::string& destinationPath,
                        const std::function<void(std::uint64_t, std::uint64_t)>& onProgress = {});

std::string getText(const std::string& url,
                    const std::list<std::string>& headers = {});

} // namespace themeshop::http
